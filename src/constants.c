
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "macros.h"
#include "constants.h"
#include "chess.h"
#include "bitboard.h"

uint64_t king_reach_2_table[64];
uint64_t king_reach_4_table[64];
uint64_t knight_reach_2_table[64];

struct magical bitboard_magics[128];

static uint64_t
king_next_reach(uint64_t reach)
{
	reach |= east_of(reach) & ~FILE_A;
	reach |= west_of(reach) & ~FILE_H;
	reach |= north_of(reach) & ~RANK_1;
	reach |= south_of(reach) & ~RANK_8;

	return reach;
}

static void
init_king_reach_tables(void)
{
	for (int i = 0; i < 64; ++i) {
		king_reach_2_table[i] = king_next_reach(king_moves_table[i]);
		king_reach_4_table[i] = king_next_reach(king_reach_2_table[i]);
		king_reach_4_table[i] = king_next_reach(king_reach_4_table[i]);
	}
}

static void
init_knight_reach_tables(void)
{
	for (int i = 0; i < 64; ++i) {
		uint64_t src = knight_moves_table[i];

		knight_reach_2_table[i] = src;
		for (; is_nonempty(src); src = reset_lsb(src))
			knight_reach_2_table[i] |= knight_moves_table[bsf(src)];
	}
}

static void
init_sliding_move_magics(struct magical *dst, const uint64_t *raw_info,
#ifdef SLIDING_BYTE_LOOKUP
	const uint8_t *byte_lookup_table,
#endif
	const uint64_t *table)
{
	dst->mask = raw_info[0];
	dst->multiplier = raw_info[1];
	dst->shift = (int)(raw_info[2] & 0xff);
#ifdef SLIDING_BYTE_LOOKUP
	dst->attack_table = table + raw_info[3];
	dst->attack_index_table = byte_lookup_table + (raw_info[2]>>8);
#else
	dst->attack_table = table + (raw_info[2]>>8);
#endif
}

static void
init_magic_bitboards(void)
{
#ifdef SLIDING_BYTE_LOOKUP
	static const int magic_block = 4;
#else
	static const int magic_block = 3;
#endif
	for (int i = 0; i < 64; ++i) {
		init_sliding_move_magics(rook_magics + i,
		    rook_magics_raw + magic_block * i,
#ifdef SLIDING_BYTE_LOOKUP
		    rook_attack_index8,
#endif
		    rook_magic_attacks);
		init_sliding_move_magics(bishop_magics + i,
		    bishop_magics_raw + magic_block * i,
#ifdef SLIDING_BYTE_LOOKUP
		    bishop_attack_index8,
#endif
		    bishop_magic_attacks);
	}
}

void
init_constants(void)
{
	init_king_reach_tables();
	init_knight_reach_tables();
	init_magic_bitboards();
}
