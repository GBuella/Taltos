/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2017, Gabor Buella
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

#include "move_desc.h"
#include "eval.h"
#include "position.h"

#include <cstring>

#include "SEE.inc"

namespace taltos
{

static const uint16_t md_pvalue[14] = {
	0, 0,
	1, 1,
	5, 5,
	0, 0,
	3, 3,
	3, 3,
	9, 9
};

static uint64_t
count_code(bitboard pieces, int max_count)
{
	assert(max_count >= 1 || max_count <= 3);

	uint64_t count;

	if (is_nonempty(pieces))
		count = 1;
	else
		count = 0;

	if (max_count > 1) {
		pieces.reset_ls1b();
		if (pieces.is_nonempty())
			++count;
	}

	if (max_count > 2) {
		pieces.reset_ls1b();
		if (pieces.is_nonempty())
			++count;
	}

	return count;
}

static uint64_t
attack_code(const struct position *pos, bitboard pieces, int player)
{
	uint64_t code;
	int opp = opponent_of(player);

	code = count_code(pieces & pos->map[king | opp], 1);

	code *= 2;
	code += count_code(pieces & pos->map[queen | opp], 1);

	code *= 3;
	code += count_code(pieces & pos->map[rook | opp], 2);

	code *= 4;
	code += count_code(pieces & pos->nb[opp], 3);

	code *= 3;
	code += popcnt(pieces & pos->map[pawn | opp]);

	code *= 2;
	code += count_code(pieces & pos->map[king | player], 1);

	code *= 2;
	code += count_code(pieces & pos->map[queen | player], 1);

	code *= 3;
	code += count_code(pieces & pos->map[rook | player], 2);

	code *= 4;
	code += count_code(pieces & pos->nb[player], 3);

	code *= 3;
	code += popcnt(pieces & pos->map[pawn | player]);

	return code;
}

static bitboard
find_attackers(const struct move_square_desc *sq, const struct position *pos)
{
	if (sq->piece == king)
		return empty;

	bitboard attackers = empty;

	attackers |= pos->all_kings & king_pattern[sq->index];
	attackers |= pos->all_knights & knight_pattern[sq->index];
	attackers |= pos->all_rq & sq->rreach;
	attackers |= pos->all_bq & sq->breach;
	attackers |= pos->map[pawn] & pawn_attacks_south[sq->index];
	attackers |= pos->map[opponent_pawn] & pawn_attacks_north[sq->index];

	return attackers;
}

static bitboard
find_piece_attacks(const struct move_square_desc *sq)
{
	switch (sq->piece) {
	case pawn:
		return pawn_attacks_north[sq->index];
	case knight:
		return knight_pattern[sq->index];
	case bishop:
		return sq->breach;
	case rook:
		return sq->rreach;
	case queen:
		return sq->rreach | sq->breach;
	case king:
		return king_pattern[sq->index];
	default:
		unreachable;
	}
}

static void
compute_SEE_loss(struct move_square_desc *sq, const struct position *pos)
{
	if (sq->piece == king) {
		sq->SEE_loss = 0;
		return;
	}

	uint64_t code = attack_code(pos, sq->attackers, 0);

	if (SEE_values[code] < md_pvalue[sq->piece])
		sq->SEE_loss = md_pvalue[sq->piece] - SEE_values[code];
	else
		sq->SEE_loss = 0;

	sq->SEE_loss *= pawn_value;
}

static void
describe_source(struct move_desc *desc, const struct position *pos)
{
	int from = mfrom(desc->m);

	desc->src_sq.index = from;
	desc->src_sq.piece = pos->board[from];
	desc->src_sq.rreach = pos->rays[pr_rook][from];
	desc->src_sq.breach = pos->rays[pr_bishop][from];
	desc->src_sq.attacks = find_piece_attacks(&desc->src_sq);
	desc->src_sq.attacks &= pos->occupied;
	// desc->src_sq.attackers is not needed
	// desc->src_sq.attackers = find_attackers(&desc->src_sq, pos);
	desc->src_sq.SEE_loss = pawn_value * pos->hanging[from];
}

static void
describe_destination(struct move_desc *desc, const struct position *pos)
{
	int to = mto(desc->m);

	desc->dst_sq.index = to;
	desc->dst_sq.piece = mresultp(desc->m);
	desc->dst_sq.rreach = pos->rays[pr_rook][to];
	desc->dst_sq.breach = pos->rays[pr_bishop][to];

	if (desc->src_sq.rreach.is_set(to))
		desc->dst_sq.rreach |= desc->src_sq.rreach & rook_masks[to];
	else if (desc->src_sq.breach.is_set(to))
		desc->dst_sq.breach |= desc->src_sq.breach & bishop_masks[to];

	desc->dst_sq.attacks = find_piece_attacks(&desc->dst_sq);
	desc->dst_sq.attacks &= pos->occupied & ~bb(mfrom(desc->m));
	desc->dst_sq.attackers = find_attackers(&desc->dst_sq, pos);
	desc->dst_sq.attackers &= ~bb(mfrom(desc->m));
	compute_SEE_loss(&desc->dst_sq, pos);
}

static void
find_discovered_attacks(struct move_desc *desc, const struct position *pos)
{
	bitboard discovered = empty;
	bitboard reach = desc->src_sq.breach & ~desc->dst_sq.breach;

	for (int i : reach & pos->bq[0])
		discovered |= reach & bishop_masks[i];

	reach = desc->src_sq.rreach & ~desc->dst_sq.rreach;

	for (int i : reach & pos->rq[0])
		discovered |= reach & rook_masks[i];

	desc->discovered_attacks = discovered & ~pos->map[opponent_king];
	desc->discovered_check = is_nonempty(discovered & pos->map[opponent_king]);
}

static void
find_attacks(struct move_desc *desc, const struct position *pos)
{
	desc->direct_check = is_nonempty(desc->dst_sq.attacks & pos->map[opponent_king]);
	find_discovered_attacks(desc, pos);
}

static void
compute_SEE_value(struct move_desc *desc)
{
	desc->SEE_value = piece_value[mcapturedp(desc->m)];
	desc->SEE_value -= piece_value[desc->src_sq.piece];
	desc->SEE_value += piece_value[desc->dst_sq.piece];
	desc->SEE_value += desc->src_sq.SEE_loss;
	desc->SEE_value -= desc->dst_sq.SEE_loss;
}

static uint16_t
attacks_value(bitboard all_attacks, const struct position *pos)
{
	uint16_t value = 0;

	for (int i : all_attacks & pos->map[0] & pos->hanging_map)
		value += piece_value[pos->board[i]] / 3;

	for (int i : all_attacks & pos->undefended[1])
		value += piece_value[pos->board[i]] / 4;

	for (int i : all_attacks & pos->map[1] & ~pos->hanging_map)
		value += piece_value[pos->board[i]] / 18;

	return value;
}

static bool
is_passed_pawn(const struct move_square_desc *sq, const struct position *pos)
{
	if (sq->piece != pawn)
		return false;

	bitboard p = bb(sq->index);
	p &= bb_rank_7 | bb_rank_6 | bb_rank_5 | bb_rank_4;
	p &= ~pos->pawn_attack_reach[1];
	if (is_empty(p))
		return false;

	p &= ~filled_south(pos->map[1]);

	return is_nonempty(p);
}

static bool
unblocks_passed_pawn(const struct position *pos, move m)
{
	if (mresultp(m) == pawn)
		return false;

	if (ind_file(mfrom(m)) == ind_file(mto(m)))
		return false;

	bitboard p = pos->map[pawn] & south_of(bb(mto(m)));
	if (is_empty(p))
		return false;

	p &= bb_rank_7 | bb_rank_6 | bb_rank_5 | bb_rank_4;
	p &= ~pos->pawn_attack_reach[1];
	if (is_empty(p))
		return false;

	p &= ~filled_south(pos->map[1]);

	return is_nonempty(p);
}

static void
eval_piece_placement(struct move_desc *desc, const struct position *pos)
{
	static char move_dest_table[64] = {
		4, 4, 4, 4, 4, 4, 4, 4,
		3, 3, 3, 3, 3, 3, 3, 3,
		0, 1, 2, 3, 3, 2, 1, 0,
		0, 1, 2, 3, 3, 2, 1, 0,
		0, 1, 2, 3, 3, 2, 1, 0,
		0, 1, 2, 2, 2, 2, 1, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	if (desc->SEE_value < 0)
		return;

	if (desc->dst_sq.piece != king || is_empty(pos->rq[1])) {
		desc->value += move_dest_table[desc->dst_sq.index];
		desc->value -= move_dest_table[desc->src_sq.index];
	}
}

static void
eval_passed_pawn(struct move_desc *desc, const struct position *pos)
{
	if (!is_passed_pawn(&desc->dst_sq, pos))
		return;

	desc->value += 100;

	if (desc->dst_sq.SEE_loss > 0)
		return;

	desc->value += 1000;

	if (ind_rank(desc->dst_sq.index) == rank_7)
		desc->value += 50;
	else if (ind_rank(desc->dst_sq.index) == rank_6)
		desc->value += 20;
}

static void
eval_check(struct move_desc *desc, const struct position *pos)
{
	if ((desc->dst_sq.SEE_loss == 0 && desc->direct_check)
	    || desc->discovered_check) {
		desc->value += 1100;
		if (is_empty(desc->dst_sq.attackers & pos->map[1]))
			desc->value += 80;
	}
}

static bool
has_strong_attack(bitboard attacks, int piece, const struct position *pos)
{
	if (piece == queen)
		return false;

	if (is_nonempty(attacks & pos->map[opponent_queen]))
		return true;

	if (piece == rook)
		return false;

	if (is_nonempty(attacks & pos->map[opponent_rook]))
		return true;

	if (piece == pawn && is_nonempty(attacks & pos->nb[1]))
		return true;

	return false;
}

static void
eval_direct_attacks(struct move_desc *desc, const struct position *pos)
{
	bitboard new_direct = desc->dst_sq.attacks & ~desc->src_sq.attacks;

	if (desc->dst_sq.SEE_loss == 0) {
		if (has_strong_attack(new_direct, desc->dst_sq.piece, pos))
			desc->value += 90;
	}

	if (desc->discovered_check or not pos->attack[1].is_set(mto(desc->m)))
		desc->value += attacks_value(new_direct, pos);
	else if (desc->dst_sq.SEE_loss == 0)
		desc->value += attacks_value(new_direct, pos) / 2;

	bitboard old_direct = desc->src_sq.attacks & ~desc->dst_sq.attacks;

	if (desc->src_sq.SEE_loss == 0) {
		if (has_strong_attack(old_direct, desc->src_sq.piece, pos))
			desc->value -= 80;
	}
}

static void
eval_discovered_attacks(struct move_desc *desc, const struct position *pos)
{
	if (desc->direct_check or not pos->attack[1].is_set(mto(desc->m)))
		desc->value += attacks_value(desc->discovered_attacks, pos);
	else if (desc->dst_sq.SEE_loss == 0)
		desc->value += attacks_value(desc->discovered_attacks, pos) / 3;
}

static void
eval_passed_pawn_unblock(struct move_desc *desc, const struct position *pos)
{
	if (desc->SEE_value >= 0 && unblocks_passed_pawn(pos, desc->m))
		desc->value += 130;
}

void
describe_move(struct move_desc *desc, const struct position *pos, move m)
{
	move prev_m = desc->m;

	if (prev_m == m)
		return;

	desc->m = m;

	if (prev_m == 0 || (mfrom(prev_m) != mfrom(m)))
		describe_source(desc, pos);

	describe_destination(desc, pos);
	find_attacks(desc, pos);
	compute_SEE_value(desc);
	desc->value = desc->SEE_value;
	if (desc->dst_sq.SEE_loss == 0 && is_capture(m))
		desc->value += 1000;

	eval_direct_attacks(desc, pos);
	eval_discovered_attacks(desc, pos);
	eval_piece_placement(desc, pos);
	eval_check(desc, pos);
	eval_passed_pawn(desc, pos);
	eval_passed_pawn_unblock(desc, pos);
}

void
find_hanging_pieces(struct position *pos)
{
	memset(pos->hanging, 0, sizeof(pos->hanging));
	pos->hanging_map = empty;

	bitboard pieces = pos->map[0] & pos->attack[1];
	pieces |= pos->map[1] & pos->attack[0];
	pieces &= ~pos->all_kings;

	for (; is_nonempty(pieces); pieces.reset_ls1b()) {
		struct move_square_desc sq;

		sq.index = pieces.ls1b_index();
		sq.piece = pos->board[sq.index];
		sq.rreach = pos->rays[pr_rook][sq.index];
		sq.breach = pos->rays[pr_bishop][sq.index];
		int player = pos_player_at(pos, sq.index);
		bitboard attackers = find_attackers(&sq, pos);
		uint64_t code = attack_code(pos, attackers, player);
		if (SEE_values[code] < md_pvalue[sq.piece]) {
			pos->hanging[sq.index] =
			    md_pvalue[sq.piece] - SEE_values[code];
			pos->hanging_map |= pieces.ls1b();
		}
	}
}

}
