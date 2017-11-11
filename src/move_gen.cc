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

#include <cassert>
#include <cstdint>
#include <cstring>

#include "macros.h"
#include "bitboard.h"
#include "chess.h"
#include "position.h"

namespace taltos
{

struct move_gen {
	const struct position *pos;
	move *moves;
	bitboard dst_mask;
	bitboard pinned_ver;
	bitboard pinned_hor;
	bitboard pinned_diag;
	bitboard pinned_adiag;
	bool only_queen_promotions;
};

static void
append_gmove_noc(struct move_gen *mg, int from, int to, int piece)
{
	*mg->moves++ = create_move_g(from, to, piece, 0);
}

static void
append_gmove(struct move_gen *mg, int from, int to, int piece)
{
	int captured = mg->pos->board[to];
	*mg->moves++ = create_move_g(from, to, piece, captured);
}

static void
append_ep(struct move_gen *mg, int from)
{
	*mg->moves++ = create_move_ep(from, mg->pos->ep_index + NORTH);
}

static void
append_pd(struct move_gen *mg, int to)
{
	*mg->moves++ = create_move_pd(to + SOUTH + SOUTH, to);
}

static void
append_promotions(struct move_gen *mg, int from, int to)
{
	int captured = mg->pos->board[to];

	*mg->moves++ = create_move_pr(from, to, queen, captured);

	if (mg->only_queen_promotions)
		return;

	*mg->moves++ = create_move_pr(from, to, knight, captured);
	*mg->moves++ = create_move_pr(from, to, bishop, captured);
	*mg->moves++ = create_move_pr(from, to, rook, captured);
}

static void
append_gmoves(struct move_gen *mg, int from, bitboard to_map, int piece)
{
	for (int to : to_map)
		append_gmove(mg, from, to, piece);
}

static void
gen_king_moves(struct move_gen *mg, bitboard dst_mask)
{
	bitboard dsts = mg->pos->attack[king];
	dsts &= dst_mask;
	dsts &= ~mg->pos->attack[1];
	dsts &= ~mg->pos->king_danger_map;

	append_gmoves(mg, mg->pos->ki, dsts, king);
}

static void
gen_castle_queen_side(struct move_gen *mg)
{
	assert(!is_in_check(mg->pos));

	if (!mg->pos->cr_queen_side)
		return;

	if (mg->pos->occupied.is_any_set(b1, c1, d1))
		return;

	if (mg->pos->attack[1].is_any_set(c1, d1))
		return;

	*mg->moves++ = mcastle_queen_side;
}

static void
gen_castle_king_side(struct move_gen *mg)
{
	assert(!is_in_check(mg->pos));

	if (!mg->pos->cr_king_side)
		return;

	if (mg->pos->occupied.is_any_set(f1, g1))
		return;

	if (mg->pos->attack[1].is_any_set(f1, g1))
		return;

	*mg->moves++ = mcastle_king_side;
}

static bool
is_ep_pinned_horizontally(const struct move_gen *mg, bitboard attackers)
{
	if (not attackers.is_singular())
		return false;

	bitboard ray = hor_reach(mg->pos, attackers.ls1b_index());
	ray |= hor_reach(mg->pos, mg->pos->ep_index);

	if (is_nonempty(ray & mg->pos->map[king])) {
		if (is_nonempty(ray & mg->pos->rq[1]))
			return true;
	}

	return false;
}

static void
gen_en_passant(struct move_gen *mg)
{
	if (!pos_has_ep_target(mg->pos))
		return;

	bitboard victim = bb(mg->pos->ep_index);
	bitboard to64 = north_of(victim);

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

	bitboard attackers = pawn_reach_south(to64) & mg->pos->map[pawn];

	if (is_ep_pinned_horizontally(mg, attackers))
		return;

	attackers &= ~mg->pinned_ver;

	if (is_nonempty(attackers & east_of(victim) & ~mg->pinned_adiag))
		append_ep(mg, mg->pos->ep_index + EAST);

	if (is_nonempty(attackers & west_of(victim) & ~mg->pinned_diag))
		append_ep(mg, mg->pos->ep_index + WEST);
}

static void
gen_pawn_pushes(struct move_gen *mg)
{
	bitboard pawns = mg->pos->map[pawn];
	pawns &= ~mg->pinned_hor;
	pawns &= ~mg->pinned_diag;
	pawns &= ~mg->pinned_adiag;

	bitboard pushes = north_of(pawns) & ~mg->pos->occupied & mg->dst_mask;

	for (int to : pushes) {
		int from = to + SOUTH;

		if (ind_rank(to) == rank_8)
			append_promotions(mg, from, to);
		else
			append_gmove_noc(mg, from, to, pawn);
	}

	pawns &= bb_rank_2;

	pushes = north_of(pawns) & ~mg->pos->occupied;
	pushes = north_of(pushes) & mg->dst_mask & ~mg->pos->occupied;

	for (int to : pushes)
		append_pd(mg, to);
}

static void
gen_pawn_captures(struct move_gen *mg)
{
	bitboard pawns = mg->pos->map[pawn];
	pawns &= ~mg->pinned_hor;
	pawns &= ~mg->pinned_ver;

	bitboard victims = mg->pos->map[1] & mg->dst_mask;
	victims &= north_of(west_of(pawns & ~mg->pinned_adiag & ~bb_file_a));

	for (int to : victims) {
		int from = to + SOUTH + EAST;

		if (ind_rank(to) == rank_8)
			append_promotions(mg, from, to);
		else
			append_gmove(mg, from, to, pawn);
	}

	victims = mg->pos->map[1] & mg->dst_mask;
	victims &= north_of(east_of(pawns & ~mg->pinned_diag & ~bb_file_h));

	for (int to : victims) {
		int from = to + SOUTH + WEST;

		if (ind_rank(to) == rank_8)
			append_promotions(mg, from, to);
		else
			append_gmove(mg, from, to, pawn);
	}
}

static void
gen_knight_moves(struct move_gen *mg)
{
	for (int from : mg->pos->map[knight] & ~mg->pos->king_pins[0]) {
		bitboard dsts = knight_pattern[from] & mg->dst_mask;

		append_gmoves(mg, from, dsts, knight);
	}
}

static void
gen_bishop_moves(struct move_gen *mg)
{
	bitboard bishops = mg->pos->map[bishop];
	bishops &= ~mg->pinned_hor;
	bishops &= ~mg->pinned_ver;

	for (; is_nonempty(bishops); bishops.reset_ls1b()) {
		bitboard from64 = bishops.ls1b();
		int from = bishops.ls1b_index();
		bitboard dst_map = mg->pos->rays[pr_bishop][from];
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
	bitboard rooks = mg->pos->map[rook];
	rooks &= ~mg->pinned_diag;
	rooks &= ~mg->pinned_adiag;

	for (; is_nonempty(rooks); rooks.reset_ls1b()) {
		bitboard from64 = rooks.ls1b();
		int from = rooks.ls1b_index();

		bitboard dst_map = mg->pos->rays[pr_rook][from];
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
	bitboard queens = mg->pos->map[queen];

	for (; is_nonempty(queens); queens.reset_ls1b()) {
		bitboard from64 = queens.ls1b();
		int from = queens.ls1b_index();
		bitboard dst_map;
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
init_data(struct move_gen *mg, const struct position *pos, move moves[])
{
	mg->pos = pos;
	mg->moves = moves;

	mg->pinned_hor = pos->king_pins[0] & hor_masks[pos->ki];
	mg->pinned_ver = pos->king_pins[0] & ver_masks[pos->ki];
	mg->pinned_diag = pos->king_pins[0] & diag_masks[pos->ki];
	mg->pinned_adiag = pos->king_pins[0] & adiag_masks[pos->ki];
}

unsigned
gen_moves(const struct position *pos, move moves[])
{
	struct move_gen mg[1];

	init_data(mg, pos, moves);
	mg->only_queen_promotions = false;
	if (popcnt(pos_king_attackers(pos)) <= 1) {
		if (is_in_check(pos)) {
			mg->dst_mask = pos->king_attack_map;
		}
		else {
			mg->dst_mask = ~pos->map[0];
			gen_castle_king_side(mg);
			gen_castle_queen_side(mg);
		}
		gen_knight_moves(mg);
		gen_rook_moves(mg);
		gen_bishop_moves(mg);
		gen_queen_moves(mg);
		gen_pawn_captures(mg);
		gen_pawn_pushes(mg);
		gen_en_passant(mg);
	}

	gen_king_moves(mg, ~pos->map[0]);
	*mg->moves = 0;

	return mg->moves - moves;
}

unsigned
gen_captures(const struct position *pos, move moves[])
{
	assert(!is_in_check(pos));

	if (is_empty(pos->attack[0] & pos->map[1]) && !pos_has_ep_target(pos)) {
		*moves = 0;
		return 0;
	}

	struct move_gen mg[1];

	init_data(mg, pos, moves);
	mg->only_queen_promotions = true;
	mg->dst_mask = pos->map[1];
	if (is_nonempty(pos->attack[0] & mg->dst_mask)) {
		gen_knight_moves(mg);
		gen_rook_moves(mg);
		gen_bishop_moves(mg);
		gen_queen_moves(mg);
		gen_pawn_captures(mg);
		gen_pawn_pushes(mg);
		gen_king_moves(mg, pos->map[1]);
	}
	gen_en_passant(mg);
	*mg->moves = 0;

	return mg->moves - moves;
}

}
