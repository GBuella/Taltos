/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2018, Gabor Buella
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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"
#include "bitboard.h"
#include "chess.h"
#include "position.h"

struct move_gen {
	const struct position *pos;
	struct move *moves;
	uint64_t dst_mask;
	uint64_t pinned_ver;
	uint64_t pinned_hor;
	uint64_t pinned_diag;
	uint64_t pinned_adiag;
	bool only_queen_promotions;
};

static void
append_move(struct move_gen *mg, int from, int to, int piece, int captured,
	    enum move_type type)
{
	invariant(ivalid(from));
	invariant(ivalid(to));
	invariant(is_valid_piece(piece));
	invariant(captured == 0 || is_valid_piece(captured));
	invariant(is_valid_mt(type));

	*mg->moves++ = (struct move){.from = from, .to = to,
			.result = piece, .captured = captured,
			.type = type, .reserved = 0};
}

static void
append_gmove_noc(struct move_gen *mg, int from, int to, int piece)
{
	append_move(mg, from, to, piece, 0, mt_general);
}

static void
append_gmove(struct move_gen *mg, int from, int to, int piece)
{
	append_move(mg, from, to, piece, mg->pos->board[to], mt_general);
}

static void
append_ep(struct move_gen *mg, int from, int to)
{
	append_move(mg, from, to, pawn, pawn, mt_en_passant);
}

static void
append_pd(struct move_gen *mg, int from, int to)
{
	append_move(mg, from, to, pawn, 0, mt_pawn_double_push);
}

static void
append_pr(struct move_gen *mg, int from, int to, int result, int captured)
{
	append_move(mg, from, to, result, captured, mt_promotion);
}

static void
append_promotions(struct move_gen *mg, int from, int to)
{
	int captured = mg->pos->board[to];

	append_pr(mg, from, to, queen, captured);

	if (mg->only_queen_promotions)
		return;

	append_pr(mg, from, to, knight, captured);
	append_pr(mg, from, to, bishop, captured);
	append_pr(mg, from, to, rook, captured);
}

static void
append_gmoves(struct move_gen *mg, int from, uint64_t to_map, int piece)
{
	for (; is_nonempty(to_map); to_map = reset_lsb(to_map))
		append_gmove(mg, from, bsf(to_map), piece);
}

static void
gen_king_moves(struct move_gen *mg, uint64_t dst_mask)
{
	uint64_t dsts = mg->pos->attack[mg->pos->turn | king];
	dsts &= dst_mask;
	dsts &= ~mg->pos->attack[mg->pos->opponent];
	dsts &= ~mg->pos->king_danger_map;

	append_gmoves(mg, bsf(mg->pos->map[mg->pos->turn | king]), dsts, king);
}

static void
gen_white_castle_queen_side(struct move_gen *mg)
{
	assert(!is_in_check(mg->pos));

	if (!mg->pos->cr_white_queen_side)
		return;

	if (is_nonempty((SQ_B1|SQ_C1|SQ_D1) & mg->pos->occupied))
		return;

	if (is_nonempty((SQ_C1|SQ_D1) & mg->pos->attack[black]))
		return;

	*mg->moves++ = mcastle_white_queen_side();
}

static void
gen_white_castle_king_side(struct move_gen *mg)
{
	assert(!is_in_check(mg->pos));

	if (!mg->pos->cr_white_king_side)
		return;

	if (is_nonempty((SQ_F1|SQ_G1) & mg->pos->occupied))
		return;

	if (is_nonempty((SQ_F1|SQ_G1) & mg->pos->attack[black]))
		return;

	*mg->moves++ = mcastle_white_king_side();
}

static void
gen_black_castle_queen_side(struct move_gen *mg)
{
	assert(!is_in_check(mg->pos));

	if (!mg->pos->cr_black_queen_side)
		return;

	if (is_nonempty((SQ_B8|SQ_C8|SQ_D8) & mg->pos->occupied))
		return;

	if (is_nonempty((SQ_C8|SQ_D8) & mg->pos->attack[white]))
		return;

	*mg->moves++ = mcastle_black_queen_side();
}

static void
gen_black_castle_king_side(struct move_gen *mg)
{
	assert(!is_in_check(mg->pos));

	if (!mg->pos->cr_black_king_side)
		return;

	if (is_nonempty((SQ_F8|SQ_G8) & mg->pos->occupied))
		return;

	if (is_nonempty((SQ_F8|SQ_G8) & mg->pos->attack[white]))
		return;

	*mg->moves++ = mcastle_black_king_side();
}

static bool
is_ep_pinned_horizontally(const struct move_gen *mg, uint64_t attackers)
{
	if (!is_singular(attackers))
		return false;

	uint64_t ray = hor_reach(mg->pos, bsf(attackers));
	ray |= hor_reach(mg->pos, mg->pos->ep_index);

	if (is_nonempty(ray & mg->pos->map[mg->pos->turn | king])) {
		if (is_nonempty(ray & mg->pos->rq[mg->pos->opponent]))
			return true;
	}

	return false;
}

static void
gen_white_en_passant(struct move_gen *mg)
{
	if (!pos_has_ep_index(mg->pos))
		return;

	uint64_t victim = bit64(mg->pos->ep_index);
	uint64_t to64 = north_of(victim);

	/*
	 * While in check
	 * capture the pawn attacking the king,
	 * or block an attack.
	 */
	if (is_empty(mg->dst_mask & (to64 | victim)))
		return;

	/*
	 * Can't make the move if removing the captured piece
	 * would reveal a check by bishop or queen.
	 */
	if (is_nonempty(mg->pinned_diag & victim))
		return;

	if (is_nonempty(mg->pinned_adiag & victim))
		return;

	uint64_t attackers = pawn_reach_south(to64) & mg->pos->map[white_pawn];

	if (is_ep_pinned_horizontally(mg, attackers))
		return;

	attackers &= ~mg->pinned_ver;

	if (is_nonempty(attackers & east_of(victim) & ~mg->pinned_adiag))
		append_ep(mg, mg->pos->ep_index + EAST,
			  mg->pos->ep_index + NORTH);

	if (is_nonempty(attackers & west_of(victim) & ~mg->pinned_diag))
		append_ep(mg, mg->pos->ep_index + WEST,
			  mg->pos->ep_index + NORTH);
}

static void
gen_black_en_passant(struct move_gen *mg)
{
	if (!pos_has_ep_index(mg->pos))
		return;

	uint64_t victim = bit64(mg->pos->ep_index);
	uint64_t to64 = south_of(victim);

	/*
	 * While in check
	 * capture the pawn attacking the king,
	 * or block an attack.
	 */
	if (is_empty(mg->dst_mask & (to64 | victim)))
		return;

	/*
	 * Can't make the move if removing the captured piece
	 * would reveal a check by bishop or queen.
	 */
	if (is_nonempty(mg->pinned_diag & victim))
		return;

	if (is_nonempty(mg->pinned_adiag & victim))
		return;

	uint64_t attackers = pawn_reach_north(to64) & mg->pos->map[black_pawn];

	if (is_ep_pinned_horizontally(mg, attackers))
		return;

	attackers &= ~mg->pinned_ver;

	if (is_nonempty(attackers & east_of(victim) & ~mg->pinned_diag))
		append_ep(mg, mg->pos->ep_index + EAST,
			  mg->pos->ep_index + SOUTH);

	if (is_nonempty(attackers & west_of(victim) & ~mg->pinned_adiag))
		append_ep(mg, mg->pos->ep_index + WEST,
			  mg->pos->ep_index + SOUTH);
}

static void
gen_white_pawn_pushes(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[white_pawn];
	pawns &= ~mg->pinned_hor;
	pawns &= ~mg->pinned_diag;
	pawns &= ~mg->pinned_adiag;

	uint64_t pushes = ~mg->pos->occupied & mg->dst_mask;

	pushes &= north_of(pawns);

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		uint64_t to = bsf(pushes);
		uint64_t from = to + SOUTH;

		if (ind_rank(to) == rank_8)
			append_promotions(mg, from, to);
		else
			append_gmove_noc(mg, from, to, pawn);
	}

	pawns &= RANK_2;

	pushes = north_of(pawns) & ~mg->pos->occupied;
	pushes = north_of(pushes) & mg->dst_mask & ~mg->pos->occupied;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes))
		append_pd(mg, bsf(pushes) + SOUTH + SOUTH, bsf(pushes));
}

static void
gen_black_pawn_pushes(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[black_pawn];
	pawns &= ~mg->pinned_hor;
	pawns &= ~mg->pinned_diag;
	pawns &= ~mg->pinned_adiag;

	uint64_t pushes = ~mg->pos->occupied & mg->dst_mask;

	pushes &= south_of(pawns);

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		uint64_t to = bsf(pushes);
		uint64_t from = to + NORTH;

		if (ind_rank(to) == rank_1)
			append_promotions(mg, from, to);
		else
			append_gmove_noc(mg, from, to, pawn);
	}

	pawns &= RANK_7;

	pushes = south_of(pawns) & ~mg->pos->occupied;
	pushes = south_of(pushes) & mg->dst_mask & ~mg->pos->occupied;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes))
		append_pd(mg, bsf(pushes) + NORTH + NORTH, bsf(pushes));
}

static void
gen_white_pawn_captures(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[white_pawn];
	pawns &= ~mg->pinned_hor;
	pawns &= ~mg->pinned_ver;

	uint64_t victims = mg->pos->map[black] & mg->dst_mask;
	victims &= north_of(west_of(pawns & ~mg->pinned_adiag & ~FILE_A));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		int to = bsf(victims);
		int from = to + SOUTH + EAST;

		if (ind_rank(to) == rank_8)
			append_promotions(mg, from, to);
		else
			append_gmove(mg, from, to, pawn);
	}

	victims = mg->pos->map[black] & mg->dst_mask;
	victims &= north_of(east_of(pawns & ~mg->pinned_diag & ~FILE_H));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		int to = bsf(victims);
		int from = to + SOUTH + WEST;

		if (ind_rank(to) == rank_8)
			append_promotions(mg, from, to);
		else
			append_gmove(mg, from, to, pawn);
	}
}

static void
gen_black_pawn_captures(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[black_pawn];
	pawns &= ~mg->pinned_hor;
	pawns &= ~mg->pinned_ver;

	uint64_t victims = mg->pos->map[white] & mg->dst_mask;
	victims &= south_of(west_of(pawns & ~mg->pinned_diag & ~FILE_A));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		int to = bsf(victims);
		int from = to + NORTH + EAST;

		if (ind_rank(to) == rank_1)
			append_promotions(mg, from, to);
		else
			append_gmove(mg, from, to, pawn);
	}

	victims = mg->pos->map[white] & mg->dst_mask;
	victims &= south_of(east_of(pawns & ~mg->pinned_adiag & ~FILE_H));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		int to = bsf(victims);
		int from = to + NORTH + WEST;

		if (ind_rank(to) == rank_1)
			append_promotions(mg, from, to);
		else
			append_gmove(mg, from, to, pawn);
	}
}

static void
gen_knight_moves(struct move_gen *mg)
{
	uint64_t knights = mg->pos->map[mg->pos->turn + knight];
	knights &= ~mg->pos->king_pins[mg->pos->turn];

	for (; is_nonempty(knights); knights = reset_lsb(knights)) {
		int from = bsf(knights);

		uint64_t dsts = knight_pattern[from] & mg->dst_mask;

		append_gmoves(mg, from, dsts, knight);
	}
}

static void
gen_bishop_moves(struct move_gen *mg)
{
	uint64_t bishops = mg->pos->map[mg->pos->turn + bishop];
	bishops &= ~mg->pinned_hor;
	bishops &= ~mg->pinned_ver;

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		uint64_t from64 = lsb(bishops);
		int from = bsf(bishops);
		uint64_t dst_map = bishop_reach(mg->pos, from);
		if (is_nonempty(mg->pinned_diag & from64))
			dst_map &= diag_masks[from];
		else if (is_nonempty(mg->pinned_adiag & from64))
			dst_map &= adiag_masks[from];
		dst_map &= mg->dst_mask;

		append_gmoves(mg, from, dst_map, bishop);
	}
}

static void
gen_rook_moves(struct move_gen *mg)
{
	uint64_t rooks = mg->pos->map[mg->pos->turn + rook];
	rooks &= ~mg->pinned_diag;
	rooks &= ~mg->pinned_adiag;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		uint64_t from64 = lsb(rooks);
		int from = bsf(rooks);

		uint64_t dst_map = rook_reach(mg->pos, from);
		if (is_nonempty(mg->pinned_hor & from64))
			dst_map &= hor_masks[from];
		else if (is_nonempty(mg->pinned_ver & from64))
			dst_map &= ver_masks[from];
		dst_map &= mg->dst_mask;

		append_gmoves(mg, from, dst_map, rook);
	}
}

static void
gen_queen_moves(struct move_gen *mg)
{
	uint64_t queens = mg->pos->map[mg->pos->turn + queen];

	for (; is_nonempty(queens); queens = reset_lsb(queens)) {
		uint64_t from64 = lsb(queens);
		int from = bsf(queens);
		uint64_t dst_map;
		if (is_nonempty(mg->pinned_hor & from64)) {
			dst_map = hor_reach(mg->pos, from);
		}
		else if (is_nonempty(mg->pinned_ver & from64)) {
			dst_map = ver_reach(mg->pos, from);
		}
		else if (is_nonempty(mg->pinned_diag & from64)) {
			dst_map = diag_reach(mg->pos, from);
		}
		else if (is_nonempty(mg->pinned_adiag & from64)) {
			dst_map = adiag_reach(mg->pos, from);
		}
		else {
			dst_map = bishop_reach(mg->pos, from);
			dst_map |= rook_reach(mg->pos, from);
		}

		dst_map &= mg->dst_mask;

		append_gmoves(mg, from, dst_map, queen);
	}
}

static void
init_data(struct move_gen *mg, const struct position *pos,
	  struct move moves[static MOVE_ARRAY_LENGTH])
{
	mg->pos = pos;
	mg->moves = moves;

	int ki = bsf(pos->map[pos->turn | king]);
	mg->pinned_hor = pos->king_pins[pos->turn] & hor_masks[ki];
	mg->pinned_ver = pos->king_pins[pos->turn] & ver_masks[ki];
	mg->pinned_diag = pos->king_pins[pos->turn] & diag_masks[ki];
	mg->pinned_adiag = pos->king_pins[pos->turn] & adiag_masks[ki];
}

unsigned
gen_moves(const struct position *pos,
	  struct move moves[static MOVE_ARRAY_LENGTH])
{
	struct move_gen mg[1];

	init_data(mg, pos, moves);
	mg->only_queen_promotions = false;
	if (popcnt(pos_king_attackers(pos)) <= 1) {
		if (is_in_check(pos)) {
			mg->dst_mask = pos->king_attack_map;
		}
		else {
			mg->dst_mask = ~pos->map[pos->turn];
			if (pos->turn == white) {
				gen_white_castle_king_side(mg);
				gen_white_castle_queen_side(mg);
			}
			else {
				gen_black_castle_king_side(mg);
				gen_black_castle_queen_side(mg);
			}
		}
		gen_knight_moves(mg);
		gen_rook_moves(mg);
		gen_bishop_moves(mg);
		gen_queen_moves(mg);
		if (pos->turn == white) {
			gen_white_pawn_captures(mg);
			gen_white_pawn_pushes(mg);
			gen_white_en_passant(mg);
		}
		else {
			gen_black_pawn_captures(mg);
			gen_black_pawn_pushes(mg);
			gen_black_en_passant(mg);
		}
	}

	gen_king_moves(mg, ~pos->map[pos->turn]);
	*mg->moves = null_move();

	return mg->moves - moves;
}

unsigned
gen_captures(const struct position *pos,
	     struct move moves[static MOVE_ARRAY_LENGTH])
{
	assert(!is_in_check(pos));

	if (is_empty(pos->attack[pos->turn] & pos->map[pos->opponent])
	    && !pos_has_ep_index(pos)) {
		*moves = null_move();
		return 0;
	}

	struct move_gen mg[1];

	init_data(mg, pos, moves);
	mg->only_queen_promotions = true;
	mg->dst_mask = pos->map[pos->opponent];
	if (is_nonempty(pos->attack[pos->turn] & mg->dst_mask)) {
		gen_knight_moves(mg);
		gen_rook_moves(mg);
		gen_bishop_moves(mg);
		gen_queen_moves(mg);
		if (pos->turn == white) {
			gen_white_pawn_captures(mg);
			gen_white_pawn_pushes(mg);
		}
		else {
			gen_black_pawn_captures(mg);
			gen_black_pawn_pushes(mg);
		}
		gen_king_moves(mg, mg->dst_mask);
	}
	if (pos->turn == white)
		gen_white_en_passant(mg);
	else
		gen_black_en_passant(mg);
	*mg->moves = null_move();

	return mg->moves - moves;
}
