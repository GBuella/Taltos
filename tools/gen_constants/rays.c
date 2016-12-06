
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include "simple_tables.h"

#include "chess.h"
#include "bitboard.h"

#include <string.h>



/*
 * Generate a table of rays, that can be indexed by coordinates.
 * E.g.: table[0][36] should be:
 *
 * ........
 * ......1.
 * .....1..
 * ....1...
 * ........
 * ........
 * ........
 * ........
 *
 * table[39][33] should be:
 *
 * ........
 * ........
 * ........
 * ........
 * .11111..
 * ........
 * ........
 * ........
 *
 * If horizontal, vertical, or diagonal ray is possible between
 * square indexes a and b, table[a][b] should be empty.
 */

static void add_ray_betweens(uint64_t table[static 64 * 64],
			int r, int f, int r_dir, int f_dir);

void
gen_ray_between_constants(uint64_t table[static 64 * 64])
{
	memset(table, 0, 64 * 64 * sizeof(table[0]));

	for (int r = rank_8; is_valid_rank(r); r += RSOUTH) {
		for (int f = file_a; is_valid_file(f); f += EAST) {
			// horizontal
			add_ray_betweens(table, r, f, RSOUTH, 0);

			// vertical
			add_ray_betweens(table, r, f, 0, WEST);

			// diagonal
			add_ray_betweens(table, r, f, RSOUTH, EAST);

			// antidiagonal
			add_ray_betweens(table, r, f, RSOUTH, WEST);
		}
	}
}

/*
 * Generate a ray from a starting rank and file (r, f) in the given
 * direction. The ray is written into the array at each step of generating it,
 * with the appropriate indexes.
 */
static void
add_ray_betweens(uint64_t table[64 * 64], int r, int f, int r_dir, int f_dir)
{
	int src_i = ind(r, f);
	uint64_t ray = EMPTY;
	int dst_r = r + r_dir;
	int dst_f = f + f_dir;

	while (is_valid_rank(dst_r) && is_valid_file(dst_f)) {
		int dst_i = ind(dst_r, dst_f);

		/*
		 * The ray variable holds the ray between src_i and dst_i ( the
		 * bits corresponding to src_i and dst_i are zero ).
		 */
		table[src_i*64 + dst_i] = ray;
		table[dst_i*64 + src_i] = ray;

		/*
		 * Advance the dst_i square. The previous dst_i square is part of
		 * the ray between src_i and the new dst_i.
		 */
		ray |= bit64(dst_i);
		dst_r += r_dir;
		dst_f += f_dir;
	}
}



/*
 * gen_move_pattern - generate a bitboard of all sqaures a
 * rook/bishop/queen can reach on an empty chessboard from
 * a specific square. This considers occupancy as well.
 * E.g.: the four rays corresponding to a bishop on F6:
 *
 * on empty	occupancy	result:
 * board:	bitboard:
 * ...1...1	.1111...	.......1
 * ....1.1.	.1111...	....1.1.
 * ........	.1111...	........
 * ....1.1.	........	....1.1.
 * ...1...1	........	...1...1
 * ..1.....	11111111	..1.....
 * .1......	........	........
 * 1.......	........	........
 *
 * As seen in the 'result' example, the bishop can reach the
 * occupied square, but can't jump over it.
 */

static uint64_t gen_ray(int src_i, uint64_t occ, int dir, uint64_t edge);

uint64_t
gen_move_pattern(int src_i,
                 uint64_t occ,
                 const int dirs[static restrict 4],
                 const uint64_t edges[static restrict 4])
{
    return gen_ray(src_i, occ, dirs[0], edges[0])
         | gen_ray(src_i, occ, dirs[1], edges[1])
         | gen_ray(src_i, occ, dirs[2], edges[2])
         | gen_ray(src_i, occ, dirs[3], edges[3]);
}

/*
 * gen_ray - used in gen_move_pattern above
 * Generate a ray of of the squares reachable by a bishop/rook/queen on
 * an empty board from a specific square, in a scpecific direction.
 * Unlike pre mask bitboards, these don't ignore the edges.
 * E.g. a bishop on F6 can reach the following squares in the
 * southwest direction:
 *
 * ........
 * ........
 * ........
 * ....1...
 * ...1....
 * ..1.....
 * .1......
 * 1.......
 *
 */
static uint64_t
gen_ray(int src_i, uint64_t occ, int dir, uint64_t edge)
{
	uint64_t result = EMPTY;
	int i = src_i + dir;
	if (!ivalid(i)) return EMPTY;
	uint64_t bit = bit64(i);

	while (is_empty(bit & edge)) {
		result |= bit;
		if (is_nonempty(occ & bit)) return result;
		i += dir;
		if (!ivalid(i)) return result;
		bit = bit64(i);
	}

	return result;
}



/*
 * A pre-mask is a mask applied to a bitboard off occupied pieces, to
 * get a bitboard of pieces relevant to computing the moves of a certian
 * piece type. For example, in the case of a bishop on square F6, the
 * relevant mask is:
 *
 * ........
 * ....1.1.
 * ........
 * ....1.1.
 * ...1....
 * ..1.....
 * .1......
 * ........
 *
 * Note: the edges are no relevant in deciding which squares a bishop
 * can reach, but the squares between the bishop and an edge of the board
 * are relevant, as any piece residing there block the bishop from
 * reaching squares behind it.
 */
static uint64_t gen_pre_mask_ray(int i, int dir, uint64_t edge);

void
gen_pre_masks(uint64_t masks[static restrict 64],
		const int dirs[static restrict 4],
		const uint64_t edges[static restrict 4])
{
    for (int i = 0; i < 64; ++i) {
	    masks[i] = 0;
	    for (int dir_i = 0; dir_i < 4; ++dir_i)
		masks[i] |= gen_pre_mask_ray(i, dirs[dir_i], edges[dir_i]);
    }
}

/*
 * Generate a pre-mask ray corresponding to starting position, and a
 * direction. E.g. masks[18] for a bishop moving in southwest direction:
 *
 * ........
 * ........
 * ........
 * ....1...
 * ...1....
 * ..1.....
 * .1......
 * ........
 *
 *
 */
static uint64_t
gen_pre_mask_ray(int i, int dir, uint64_t edge)
{
	if (is_nonempty(bit64(i) & edge))
		return 0;

	uint64_t mask = 0;
	int ti = i + dir;
	uint64_t bit = bit64(ti);

	while (is_empty(bit & edge)) {
		mask |= bit;
		ti += dir;
		bit = bit64(ti);
	}

	return mask;
}
