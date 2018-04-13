/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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
#include "constants.h"
#include "eval.h"

static inline uint64_t
white_pawn_chains(const struct position *pos)
{
	return pos->map[white_pawn] & pos->attack[white_pawn];
}

static inline uint64_t
black_pawn_chains(const struct position *pos)
{
	return pos->map[black_pawn] & pos->attack[black_pawn];
}

static inline uint64_t
white_isolated_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[white_pawn];

	pawns &= east_of(pos->half_open_files[white] & ~FILE_H);
	pawns &= west_of(pos->half_open_files[white] & ~FILE_A);

	return pawns;
}

static inline uint64_t
black_isolated_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[black_pawn];

	pawns &= east_of(pos->half_open_files[black] & ~FILE_H);
	pawns &= west_of(pos->half_open_files[black] & ~FILE_A);

	return pawns;
}

static inline uint64_t
white_blocked_pawns(const struct position *pos)
{
	return north_of(pos->map[white_pawn]) & pos->map[black];
}

static inline uint64_t
black_blocked_pawns(const struct position *pos)
{
	return south_of(pos->map[black_pawn]) & pos->map[white];
}

static inline uint64_t
white_double_pawns(const struct position *pos)
{
	return north_of(kogge_stone_north(pos->map[white_pawn])) &
	    pos->map[white_pawn];
}

static inline uint64_t
black_double_pawns(const struct position *pos)
{
	return south_of(kogge_stone_south(pos->map[black_pawn]))
	    & pos->map[black_pawn];
}

static inline uint64_t
white_backward_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[white_pawn];

	// No friendly pawns an adjacent files, next to - or behind
	pawns &= ~south_of(pos->pawn_attack_reach[white]);

	// How far can a pawn advance, without being attacked by another pawn?
	uint64_t advance = kogge_stone_north(pawns);
	advance &= ~kogge_stone_north(advance & pos->attack[black_pawn]);
	advance &= south_of(pos->attack[white_pawn]);

	// If it can't reach a square next to a friendly pawn, it is backwards
	pawns &= ~kogge_stone_south(advance);

	return pawns;
}

static inline uint64_t
black_backward_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[black_pawn];
	pawns &= ~north_of(pos->pawn_attack_reach[black]);

	uint64_t advance = kogge_stone_south(pawns);
	advance &= ~kogge_stone_south(advance & pos->attack[white_pawn]);
	advance &= north_of(pos->attack[black_pawn]);
	pawns &= ~kogge_stone_north(advance);

	return pawns;
}

static inline uint64_t
white_outposts(const struct position *pos)
{
	return pos->attack[white_pawn]
	    & ~pos->pawn_attack_reach[black]
	    & ~(RANK_8 | FILE_A | FILE_H);
}

static inline uint64_t
black_outposts(const struct position *pos)
{
	return pos->attack[black_pawn]
	    & ~pos->pawn_attack_reach[white]
	    & ~(RANK_1 | FILE_A | FILE_H);
}

static inline uint64_t
white_knight_outposts(const struct position *pos)
{
	return pos->map[white_knight] & white_outposts(pos);
}

static inline uint64_t
black_knight_outposts(const struct position *pos)
{
	return pos->map[black_knight] & black_outposts(pos);
}

static inline uint64_t
white_knight_reach_outposts(const struct position *pos)
{
	return pos->attack[white_knight] & white_outposts(pos)
	    & ~pos->map[white];
}

static inline uint64_t
black_knight_reach_outposts(const struct position *pos)
{
	return pos->attack[black_knight] & black_outposts(pos)
	    & ~pos->map[black];
}

static inline uint64_t
white_passed_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[white_pawn];
	pawns &= (RANK_7 | RANK_6 | RANK_5 | RANK_4);
	pawns &= ~pos->pawn_attack_reach[black];
	pawns &= ~kogge_stone_south(pos->map[black_pawn]);

	return pawns;
}

static inline uint64_t
black_passed_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[black_pawn];
	pawns &= (RANK_2 | RANK_3 | RANK_4 | RANK_5);
	pawns &= ~pos->pawn_attack_reach[white];
	pawns &= ~kogge_stone_north(pos->map[white_pawn]);

	return pawns;
}

static inline uint64_t
white_rooks_on_half_open_files(const struct position *pos)
{
	return pos->map[white_rook] & pos->half_open_files[white];
}

static inline uint64_t
black_rooks_on_half_open_files(const struct position *pos)
{
	return pos->map[black_rook] & pos->half_open_files[black];
}

static inline uint64_t
white_rooks_on_open_files(const struct position *pos)
{
	return pos->map[white_rook] &
	    pos->half_open_files[white] & pos->half_open_files[black];
}

static inline uint64_t
black_rooks_on_open_files(const struct position *pos)
{
	return pos->map[black_rook] &
	    pos->half_open_files[white] & pos->half_open_files[black];
}

static inline uint64_t
white_rook_batteries(const struct position *pos)
{
	return pos->map[white_rook] & pos->attack[white_rook]
	    & pos->half_open_files[white]
	    & south_of(kogge_stone_south(pos->map[white_rook]));
}

static inline uint64_t
black_rook_batteries(const struct position *pos)
{
	return pos->map[black_rook] & pos->attack[black_rook]
	    & pos->half_open_files[black]
	    & north_of(kogge_stone_north(pos->map[black_rook]));
}

static inline uint64_t
white_pawns_on_center(const struct position *pos)
{
	return pos->map[white_pawn] & CENTER_SQ;
}

static inline uint64_t
black_pawns_on_center(const struct position *pos)
{
	return pos->map[black_pawn] & CENTER_SQ;
}

static inline uint64_t
white_pawns_on_center4(const struct position *pos)
{
	return pos->map[white_pawn] & pos->attack[white_pawn]
	    & CENTER4_SQ;
}

static inline uint64_t
black_pawns_on_center4(const struct position *pos)
{
	return pos->map[black_pawn] & pos->attack[black_pawn]
	    & CENTER4_SQ;
}

static inline uint64_t
white_center4_attacks(const struct position *pos)
{
	return (pos->attack[white_knight] | pos->attack[white_bishop])
	    & CENTER4_SQ;
}

static inline uint64_t
black_center4_attacks(const struct position *pos)
{
	return (pos->attack[black_knight] | pos->attack[black_bishop])
	    & CENTER4_SQ;
}

static inline bool
white_has_bishop_pair(const struct position *pos)
{
	return is_nonempty(pos->map[white_bishop] & BLACK_SQUARES)
	    && is_nonempty(pos->map[white_bishop] & WHITE_SQUARES);
}

static inline bool
black_has_bishop_pair(const struct position *pos)
{
	return is_nonempty(pos->map[black_bishop] & BLACK_SQUARES)
	    && is_nonempty(pos->map[black_bishop] & WHITE_SQUARES);
}

static inline uint64_t
all_pawns(const struct position *pos)
{
	return pos->map[white_pawn] | pos->map[black_pawn];
}

static inline uint64_t
pawns_on_white(const struct position *pos)
{
	return WHITE_SQUARES & all_pawns(pos);
}

static inline uint64_t
pawns_on_black(const struct position *pos)
{
	return BLACK_SQUARES & all_pawns(pos);
}

static inline uint64_t
white_bishops_on_white(const struct position *pos)
{
	return WHITE_SQUARES & pos->map[white_bishop];
}

static inline uint64_t
white_bishops_on_black(const struct position *pos)
{
	return BLACK_SQUARES & pos->map[white_bishop];
}

static inline uint64_t
black_bishops_on_white(const struct position *pos)
{
	return WHITE_SQUARES & pos->map[black_bishop];
}

static inline uint64_t
black_bishops_on_black(const struct position *pos)
{
	return BLACK_SQUARES & pos->map[black_bishop];
}

static inline uint64_t
white_free_squares(const struct position *pos)
{
	uint64_t map = ~pos->attack[black];
	map |= pos->attack[white_pawn] & ~pos->attack[black_pawn];
	return map & ~pos->map[white];
}

static inline uint64_t
black_free_squares(const struct position *pos)
{
	uint64_t map = ~pos->attack[white];
	map |= pos->attack[black_pawn] & ~pos->attack[white_pawn];
	return map & ~pos->map[black];
}

static inline int
white_non_pawn_material(const struct position *pos)
{
	return pos->material_value[white] -
	    pawn_value * popcnt(pos->map[white_pawn]);
}

static inline int
black_non_pawn_material(const struct position *pos)
{
	return pos->material_value[black] -
	    pawn_value * popcnt(pos->map[black_pawn]);
}

static inline bool
bishop_c1_is_trapped(const struct position *pos)
{
	return (pos->map[white_pawn] & (SQ_B2 | SQ_D2)) == (SQ_B2 | SQ_D2);
}

static inline bool
bishop_f1_is_trapped(const struct position *pos)
{
	return (pos->map[white_pawn] & (SQ_E2 | SQ_G2)) == (SQ_E2 | SQ_G2);
}

static inline bool
bishop_c8_is_trapped(const struct position *pos)
{
	return (pos->map[black_pawn] & (SQ_B7 | SQ_D7)) == (SQ_B7 | SQ_D7);
}

static inline bool
bishop_f8_is_trapped(const struct position *pos)
{
	return (pos->map[black_pawn] & (SQ_E7 | SQ_G7)) == (SQ_E7 | SQ_G7);
}

static inline bool
white_bishop_trapped_at_a7(const struct position *pos)
{
	return is_nonempty(pos->map[white_bishop] & SQ_A7)
	    && ((pos->map[black_pawn] & (SQ_B6 | SQ_C7)) == (SQ_B6 | SQ_C7));
}

static inline bool
white_bishop_trapped_at_h7(const struct position *pos)
{
	return is_nonempty(pos->map[white_bishop] & SQ_H7)
	    && ((pos->map[black_pawn] & (SQ_G6 | SQ_F7)) == (SQ_G6 | SQ_F7));
}

static inline bool
black_bishop_trapped_at_a2(const struct position *pos)
{
	return is_nonempty(pos->map[black_bishop] & SQ_A2)
	    && ((pos->map[white_pawn] & (SQ_B3 | SQ_C2)) == (SQ_B3 | SQ_C2));
}

static inline bool
black_bishop_trapped_at_h2(const struct position *pos)
{
	return is_nonempty(pos->map[black_bishop] & SQ_H2)
	    && ((pos->map[white_pawn] & (SQ_G3 | SQ_F2)) == (SQ_G3 | SQ_F2));
}

static inline bool
white_bishop_trappable_at_a7(const struct position *pos)
{
	return is_nonempty(pos->map[white_bishop] & SQ_A7)
	    && ((pos->map[black_pawn] & (SQ_B7 | SQ_C7)) == (SQ_B7 | SQ_C7));
}

static inline bool
white_bishop_trappable_at_h7(const struct position *pos)
{
	return is_nonempty(pos->map[white_bishop] & SQ_H7)
	    && ((pos->map[black_pawn] & (SQ_G7 | SQ_F7)) == (SQ_G7 | SQ_F7));
}

static inline bool
black_bishop_trappable_at_a2(const struct position *pos)
{
	return is_nonempty(pos->map[black_bishop] & SQ_A2)
	    && ((pos->map[white_pawn] & (SQ_B2 | SQ_C2)) == (SQ_B2 | SQ_C2));
}

static inline bool
black_bishop_trappable_at_h2(const struct position *pos)
{
	return is_nonempty(pos->map[black_bishop] & SQ_H2)
	    && ((pos->map[white_pawn] & (SQ_G2 | SQ_F2)) == (SQ_G2 | SQ_F2));
}

static inline bool
rook_a1_is_trapped(const struct position *pos)
{
	if (pos->cr_white_queen_side)
		return false;

	uint64_t r = pos->map[white_rook] & (SQ_A1 | SQ_B1 | SQ_C1);
	uint64_t trap = east_of(r) | east_of(east_of(r)) | SQ_D1;
	uint64_t blockers =
	    pos->map[white_king] | pos->map[white_bishop] | pos->map[white_knight];
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[white])
	    && is_nonempty(trap & blockers);
}

static inline bool
rook_h1_is_trapped(const struct position *pos)
{
	if (pos->cr_white_king_side)
		return false;

	uint64_t r = pos->map[white_rook] & (SQ_F1 | SQ_G1 | SQ_H1);
	uint64_t trap = west_of(r) | west_of(west_of(r)) | SQ_E1;
	uint64_t blockers =
	    pos->map[white_king] | pos->map[white_bishop] | pos->map[white_knight];
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[white])
	    && is_nonempty(trap & blockers);
}

static inline bool
rook_a8_is_trapped(const struct position *pos)
{
	if (pos->cr_black_queen_side)
		return false;

	uint64_t r = pos->map[black_rook] & (SQ_A8 | SQ_B8 | SQ_C8);
	uint64_t trap = east_of(r) | east_of(east_of(r)) | SQ_D8;
	uint64_t blockers = pos->map[black_king]
	    | pos->map[black_bishop] | pos->map[black_knight];
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[black])
	    && is_nonempty(trap & blockers);
}

static inline bool
rook_h8_is_trapped(const struct position *pos)
{
	if (pos->cr_black_king_side)
		return false;

	uint64_t r = pos->map[black_rook] & (SQ_F8 | SQ_G8 | SQ_H8);
	uint64_t trap = west_of(r) | west_of(west_of(r)) | SQ_E8;
	uint64_t blockers = pos->map[black_king]
	    | pos->map[black_bishop] | pos->map[black_knight];
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[black])
	    && is_nonempty(trap & blockers);
}

static inline bool
white_knight_cornered_a8(const struct position *pos)
{
	return is_nonempty(pos->map[white_knight] & SQ_A8)
	    && is_nonempty(pos->attack[black_pawn] & SQ_B6)
	    && is_nonempty(pos->attack[black] & SQ_C7);
}

static inline bool
white_knight_cornered_h8(const struct position *pos)
{
	return is_nonempty(pos->map[white_knight] & SQ_H8)
	    && is_nonempty(pos->attack[black_pawn] & SQ_G6)
	    && is_nonempty(pos->attack[black] & SQ_F7);
}

static inline bool
black_knight_cornered_a1(const struct position *pos)
{
	return is_nonempty(pos->map[black_knight] & SQ_A1)
	    && is_nonempty(pos->attack[white_pawn] & SQ_B3)
	    && is_nonempty(pos->attack[white] & SQ_C2);
}

static inline bool
black_knight_cornered_h1(const struct position *pos)
{
	return is_nonempty(pos->map[black_knight] & SQ_H1)
	    && is_nonempty(pos->attack[white_pawn] & SQ_G3)
	    && is_nonempty(pos->attack[white] & SQ_F2);
}

#endif
