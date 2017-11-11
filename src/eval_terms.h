/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALTOS_EVAL_TERMS_H
#define TALTOS_EVAL_TERMS_H

#include "position.h"
#include "eval.h"

namespace taltos
{

static inline bitboard
pawn_chains(const struct position *pos)
{
	return pos->map[pawn] & pos->attack[pawn];
}

static inline bitboard
opponent_pawn_chains(const struct position *pos)
{
	return pos->map[opponent_pawn] & pos->attack[opponent_pawn];
}

static inline bitboard
isolated_pawns(const struct position *pos)
{
	bitboard pawns = pos->map[pawn];

	pawns &= east_of(pos->half_open_files[0] & ~bb_file_h);
	pawns &= west_of(pos->half_open_files[0] & ~bb_file_a);

	return pawns;
}

static inline bitboard
opponent_isolated_pawns(const struct position *pos)
{
	bitboard pawns = pos->map[opponent_pawn];

	pawns &= east_of(pos->half_open_files[1] & ~bb_file_h);
	pawns &= west_of(pos->half_open_files[1] & ~bb_file_a);

	return pawns;
}

static inline bitboard
blocked_pawns(const struct position *pos)
{
	return north_of(pos->map[pawn]) & pos->map[1];
}

static inline bitboard
opponent_blocked_pawns(const struct position *pos)
{
	return south_of(pos->map[opponent_pawn]) & pos->map[0];
}

static inline bitboard
double_pawns(const struct position *pos)
{
	return north_of(filled_north(pos->map[pawn])) & pos->map[pawn];
}

static inline bitboard
opponent_double_pawns(const struct position *pos)
{
	return south_of(filled_south(pos->map[opponent_pawn])) & pos->map[opponent_pawn];
}

static inline bitboard
backward_pawns(const struct position *pos)
{
	bitboard pawns = pos->map[pawn];

	// No friendly pawns an adjacent files, next to - or behind
	pawns &= ~south_of(pos->pawn_attack_reach[0]);

	// How far can a pawn advance, without being attacked by another pawn?
	bitboard advance = filled_north(pawns);
	advance &= ~filled_north(advance & pos->attack[opponent_pawn]);
	advance &= south_of(pos->attack[pawn]);

	// If it can't reach a square next to a friendly pawn, it is backwards
	pawns &= ~filled_south(advance);

	return pawns;
}

static inline bitboard
opponent_backward_pawns(const struct position *pos)
{
	bitboard pawns = pos->map[opponent_pawn];
	pawns &= ~north_of(pos->pawn_attack_reach[1]);

	bitboard advance = filled_south(pawns);
	advance &= ~filled_south(advance & pos->attack[pawn]);
	advance &= north_of(pos->attack[opponent_pawn]);
	pawns &= ~filled_north(advance);

	return pawns;
}

static inline bitboard
outposts(const struct position *pos)
{
	return pos->attack[pawn]
	    & ~pos->pawn_attack_reach[1]
	    & ~(bb_rank_8 | bb_file_a | bb_file_h);
}

static inline bitboard
opponent_outposts(const struct position *pos)
{
	return pos->attack[opponent_pawn]
	    & ~pos->pawn_attack_reach[0]
	    & ~(bb_rank_1 | bb_file_a | bb_file_h);
}

static inline bitboard
knight_outposts(const struct position *pos)
{
	return pos->map[knight] & outposts(pos);
}

static inline bitboard
opponent_knight_outposts(const struct position *pos)
{
	return pos->map[opponent_knight] & opponent_outposts(pos);
}

static inline bitboard
knight_reach_outposts(const struct position *pos)
{
	return pos->attack[knight] & outposts(pos) & ~pos->map[0];
}

static inline bitboard
opponent_knight_reach_outposts(const struct position *pos)
{
	return pos->attack[opponent_knight] & opponent_outposts(pos) & ~pos->map[1];
}

static inline bitboard
passed_pawns(const struct position *pos)
{
	bitboard pawns = pos->map[pawn];
	pawns &= bb_rank_7 | bb_rank_6 | bb_rank_5 | bb_rank_4;
	pawns &= ~pos->pawn_attack_reach[1];
	pawns &= ~filled_south(pos->map[opponent_pawn]);

	return pawns;
}

static inline bitboard
opponent_passed_pawns(const struct position *pos)
{
	bitboard pawns = pos->map[opponent_pawn];
	pawns &= bb_rank_2 | bb_rank_3 | bb_rank_4 | bb_rank_5;
	pawns &= ~pos->pawn_attack_reach[0];
	pawns &= ~filled_north(pos->map[pawn]);

	return pawns;
}

static inline bitboard
rooks_on_half_open_files(const struct position *pos)
{
	return pos->map[rook] & pos->half_open_files[0];
}

static inline bitboard
opponent_rooks_on_half_open_files(const struct position *pos)
{
	return pos->map[opponent_rook] & pos->half_open_files[1];
}

static inline bitboard
rooks_on_open_files(const struct position *pos)
{
	return pos->map[rook] & pos->half_open_files[0] & pos->half_open_files[1];
}

static inline bitboard
opponent_rooks_on_open_files(const struct position *pos)
{
	return pos->map[opponent_rook] & pos->half_open_files[0] & pos->half_open_files[1];
}

static inline bitboard
rook_batteries(const struct position *pos)
{
	return pos->map[rook] & pos->attack[rook]
	    & pos->half_open_files[0]
	    & south_of(filled_south(pos->map[rook]));
}

static inline bitboard
opponent_rook_batteries(const struct position *pos)
{
	return pos->map[opponent_rook] & pos->attack[opponent_rook]
	    & pos->half_open_files[1]
	    & north_of(filled_north(pos->map[opponent_rook]));
}

static inline bitboard
pawns_on_center(const struct position *pos)
{
	return pos->map[pawn] & center;
}

static inline bitboard
opponent_pawns_on_center(const struct position *pos)
{
	return pos->map[opponent_pawn] & center;
}

static inline bitboard
pawns_on_center4(const struct position *pos)
{
	return pos->map[pawn] & pos->attack[pawn] & center4;
}

static inline bitboard
opponent_pawns_on_center4(const struct position *pos)
{
	return pos->map[opponent_pawn] & pos->attack[opponent_pawn] & center4;
}

static inline bitboard
center4_attacks(const struct position *pos)
{
	return (pos->attack[knight] | pos->attack[bishop]) & center4;
}

static inline bitboard
opponent_center4_attacks(const struct position *pos)
{
	return (pos->attack[opponent_knight] | pos->attack[opponent_bishop]) & center4;
}

static inline bool
has_bishop_pair(const struct position *pos)
{
	return is_nonempty(pos->map[bishop] & black_squares)
	    and is_nonempty(pos->map[bishop] & white_squares);
}

static inline bool
opponent_has_bishop_pair(const struct position *pos)
{
	return is_nonempty(pos->map[opponent_bishop] & black_squares)
	    and is_nonempty(pos->map[opponent_bishop] & white_squares);
}

static inline bitboard
all_pawns(const struct position *pos)
{
	return pos->map[pawn] | pos->map[opponent_pawn];
}

static inline bitboard
pawns_on_white(const struct position *pos)
{
	return white_squares & all_pawns(pos);
}

static inline bitboard
pawns_on_black(const struct position *pos)
{
	return black_squares & all_pawns(pos);
}

static inline bitboard
bishops_on_white(const struct position *pos)
{
	return white_squares & pos->map[bishop];
}

static inline bitboard
bishops_on_black(const struct position *pos)
{
	return black_squares & pos->map[bishop];
}

static inline bitboard
opponent_bishops_on_white(const struct position *pos)
{
	return white_squares & pos->map[opponent_bishop];
}

static inline bitboard
opponent_bishops_on_black(const struct position *pos)
{
	return black_squares & pos->map[opponent_bishop];
}

static inline bitboard
free_squares(const struct position *pos)
{
	bitboard map = ~pos->attack[1];
	map |= pos->attack[pawn] & ~pos->attack[opponent_pawn];
	return map & ~pos->map[0];
}

static inline bitboard
opponent_free_squares(const struct position *pos)
{
	bitboard map = ~pos->attack[0];
	map |= pos->attack[opponent_pawn] & ~pos->attack[pawn];
	return map & ~pos->map[1];
}

static inline int
non_pawn_material(const struct position *pos)
{
	return pos->material_value - pawn_value * popcnt(pos->map[pawn]);
}

static inline int
opponent_non_pawn_material(const struct position *pos)
{
	return pos->opponent_material_value - pawn_value * popcnt(pos->map[opponent_pawn]);
}

static inline bool
bishop_c1_is_trapped(const struct position *pos)
{
	return (pos->map[pawn] & bb(b2, d2)) == bb(b2, d2);
}

static inline bool
bishop_f1_is_trapped(const struct position *pos)
{
	return (pos->map[pawn] & bb(e2, g2)) == bb(e2, g2);
}

static inline bool
opponent_bishop_c8_is_trapped(const struct position *pos)
{
	return (pos->map[opponent_pawn] & bb(b7, d7)) == bb(b7, d7);
}

static inline bool
opponent_bishop_f8_is_trapped(const struct position *pos)
{
	return (pos->map[opponent_pawn] & bb(e7, g7)) == bb(e7, g7);
}

static inline bool
bishop_trapped_at_a7(const struct position *pos)
{
	return pos->map[bishop].is_set(a7)
	    and ((pos->map[opponent_pawn] & bb(b6, c7)) == bb(b6, c7));
}

static inline bool
bishop_trapped_at_h7(const struct position *pos)
{
	return pos->map[bishop].is_set(h7)
	    and ((pos->map[opponent_pawn] & bb(g6, f7)) == bb(g6, f7));
}

static inline bool
opponent_bishop_trapped_at_a2(const struct position *pos)
{
	return pos->map[opponent_bishop].is_set(a2)
	    and ((pos->map[pawn] & bb(b3, c2)) == bb(b3, c2));
}

static inline bool
opponent_bishop_trapped_at_h2(const struct position *pos)
{
	return pos->map[opponent_bishop].is_set(h2)
	    and ((pos->map[pawn] & bb(g3, f2)) == bb(g3, f2));
}

static inline bool
bishop_trappable_at_a7(const struct position *pos)
{
	return pos->map[bishop].is_set(a7)
	    and ((pos->map[opponent_pawn] & bb(b7, c7)) == bb(b7, c7));
}

static inline bool
bishop_trappable_at_h7(const struct position *pos)
{
	return pos->map[bishop].is_set(h7)
	    and ((pos->map[opponent_pawn] & bb(g7, f7)) == bb(g7, f7));
}

static inline bool
opponent_bishop_trappable_at_a2(const struct position *pos)
{
	return pos->map[opponent_bishop].is_set(a2)
	    and ((pos->map[pawn] & bb(b2, c2)) == bb(b2, c2));
}

static inline bool
opponent_bishop_trappable_at_h2(const struct position *pos)
{
	return pos->map[opponent_bishop].is_set(h2)
	    and ((pos->map[pawn] & bb(g2, f2)) == bb(g2, f2));
}

static inline bool
rook_a1_is_trapped(const struct position *pos)
{
	if (pos->cr_queen_side)
		return false;

	bitboard r = pos->map[rook] & bb(a1, b1, c1);
	bitboard trap = east_of(r) | east_of(east_of(r)) | bb(d1);
	bitboard blockers = pos->map[king] | pos->map[bishop] | pos->map[knight];
	return is_nonempty(r)
	    and is_empty(r & pos->half_open_files[0])
	    and is_nonempty(trap & blockers);
}

static inline bool
rook_h1_is_trapped(const struct position *pos)
{
	if (pos->cr_king_side)
		return false;

	bitboard r = pos->map[rook] & bb(f1, g1, h1);
	bitboard trap = west_of(r) | west_of(west_of(r)) | bb(e1);
	bitboard blockers = pos->map[king] | pos->map[bishop] | pos->map[knight];
	return is_nonempty(r)
	    and is_empty(r & pos->half_open_files[0])
	    and is_nonempty(trap & blockers);
}

static inline bool
opponent_rook_a8_is_trapped(const struct position *pos)
{
	if (pos->cr_opponent_queen_side)
		return false;

	bitboard r = pos->map[opponent_rook] & bb(a8, b8, c8);
	bitboard trap = east_of(r) | east_of(east_of(r)) | bb(d8);
	bitboard blockers = pos->map[opponent_king]
	    | pos->map[opponent_bishop] | pos->map[opponent_knight];
	return is_nonempty(r)
	    and is_empty(r & pos->half_open_files[1])
	    and is_nonempty(trap & blockers);
}

static inline bool
opponent_rook_h8_is_trapped(const struct position *pos)
{
	if (pos->cr_opponent_king_side)
		return false;

	bitboard r = pos->map[opponent_rook] & bb(f8, g8, h8);
	bitboard trap = west_of(r) | west_of(west_of(r)) | bb(e8);
	bitboard blockers = pos->map[opponent_king]
	    | pos->map[opponent_bishop] | pos->map[opponent_knight];
	return is_nonempty(r)
	    and is_empty(r & pos->half_open_files[1])
	    and is_nonempty(trap & blockers);
}

static inline bool
knight_cornered_a8(const struct position *pos)
{
	return pos->map[knight].is_set(a8)
	    and pos->attack[opponent_pawn].is_set(b6)
	    and pos->attack[1].is_set(c7);
}

static inline bool
knight_cornered_h8(const struct position *pos)
{
	return pos->map[knight].is_set(h8)
	    and pos->attack[opponent_pawn].is_set(g6)
	    and pos->attack[1].is_set(f7);
}

static inline bool
opponent_knight_cornered_a1(const struct position *pos)
{
	return pos->map[opponent_knight].is_set(a1)
	    and pos->attack[pawn].is_set(b3)
	    and pos->attack[0].is_set(c2);
}

static inline bool
opponent_knight_cornered_h1(const struct position *pos)
{
	return pos->map[opponent_knight].is_set(h1)
	    and pos->attack[pawn].is_set(g3)
	    and pos->attack[0].is_set(f2);
}

}

#endif
