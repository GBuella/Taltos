
/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=(0,t0: */

#include "constants.h"
#include "chess.h"
#include "bitboard.h"

#include <stdlib.h>
#include <string.h>

uint64_t knight_pattern[64];
uint64_t king_pattern[64];
uint64_t diag_masks[64];
uint64_t adiag_masks[64];
uint64_t hor_masks[64];
uint64_t ver_masks[64];
uint64_t bishop_masks[64];
uint64_t rook_masks[64];

static const int king_dirs_h[] = { 1, 1, 1, 0, -1, -1, -1, 0};
static const int king_dirs_v[] = { -1, 0, 1, 1, 1, 0, -1, -1};

static const int knight_dirs_h[] = { -2, -1, -2, -1,  2,  1,  2,  1};
static const int knight_dirs_v[] = { -1, -2,  1,  2, -1, -2,  1,  2};

static const int rook_dirs[4] = {EAST, WEST, NORTH, SOUTH};

static const uint64_t rook_edges[4] = { FILE_H, FILE_A, RANK_8, RANK_1};

static const int bishop_dirs[4] = {
	EAST + SOUTH,
	WEST + NORTH,
	WEST + SOUTH,
	EAST + NORTH};

static const uint64_t bishop_edges[4] = {
	FILE_H|RANK_1,
	FILE_A|RANK_8,
	FILE_A|RANK_1,
	FILE_H|RANK_8};

static void
gen_simple_table(uint64_t table[64], const int dirs_v[8], const int dirs_h[8])
{
	for (int rank = 0; rank < 8; ++rank) {
		for (int file = 0; file < 8; ++file) {
			for (int dir_i = 0; dir_i < 8; ++dir_i) {
				int r = rank + dirs_v[dir_i];
				int f = file + dirs_h[dir_i];
				if (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
					int i = ind(r, f);
					table[ind(rank, file)] |= bit64(i);
				}
			}
		}
	}
}

static uint64_t
gen_ray(int i, int dir, uint64_t edge)
{
	uint64_t bit = bit64(i);
	uint64_t ray = EMPTY;

	while (is_empty(edge & bit)) {
		i += dir;
		bit = bit64(i);
		ray |= bit;
	}

	return ray;
}

static void
gen_ray_64(uint64_t table[64], int dir, uint64_t edge)
{
	for (int i = 0; i < 64; ++i)
		table[i] |= gen_ray(i, dir, edge);
}

static void
gen_masks(uint64_t table[64], int dir_count,
	  const int *dirs, const uint64_t *edges)
{
	for (int i = 0; i < dir_count; ++i)
		gen_ray_64(table, dirs[i], edges[i]);
}

void
init_constants(void)
{
	gen_simple_table(king_pattern, king_dirs_v, king_dirs_h);
	gen_simple_table(knight_pattern, knight_dirs_v, knight_dirs_h);
	gen_masks(diag_masks, 2, bishop_dirs, bishop_edges);
	gen_masks(adiag_masks, 2, bishop_dirs + 2, bishop_edges + 2);
	gen_masks(hor_masks, 2, rook_dirs, rook_edges);
	gen_masks(ver_masks, 2, rook_dirs + 2, rook_edges + 2);
	gen_masks(bishop_masks, 4, bishop_dirs, bishop_edges);
	gen_masks(rook_masks, 4, rook_dirs, rook_edges);
}
