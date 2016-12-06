
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include "simple_tables.h"

#include "chess.h"
#include "bitboard.h"

#include <string.h>

/*
 * gen_simple_table -- generates move destination tables for king and knight.
 * These are rather simple, compared to what must be done for ranged pieces.
 */
static void
gen_simple_table(uint64_t table[static restrict 64],
		const int dirs_v[static restrict 8],
		const int dirs_h[static restrict 8])
{
	memset(table, 0, 64 * sizeof(table[0]));

	for (int rank = 0; rank < 8; ++rank) {
		for (int file = 0; file < 8; ++file) {
			for (int dir_i = 0; dir_i < 8; ++dir_i) {
				int r = rank + dirs_v[dir_i];
				int f = file + dirs_h[dir_i];
				int i = ind(rank, file);

				// Check if the move crosses any board edge
				if (r >= 0 && r <= 7 && f >= 0 && f <= 7)
					table[i] |= bit64(ind(r, f));
			}
		}
	}
}

void
gen_knight_table(uint64_t table[static 64])
{
	static const int knight_dirs_h[] = { -2, -1, -2, -1,  2,  1,  2,  1};
	static const int knight_dirs_v[] = { -1, -2,  1,  2, -1, -2,  1,  2};

	gen_simple_table(table, knight_dirs_v, knight_dirs_h);
}

void
gen_king_table(uint64_t table[static 64])
{
	static const int king_dirs_h[] = { 1, 1, 1, 0, -1, -1, -1, 0};
	static const int king_dirs_v[] = { -1, 0, 1, 1, 1, 0, -1, -1};

	gen_simple_table(table, king_dirs_v, king_dirs_h);
}
