/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include "move_desc.h"
#include "eval.h"
#include "position.h"
#include "constants.h"

#include <string.h>

#include "SEE.inc"

static uint64_t
count_code(uint64_t pieces, int max_count)
{
	assert(max_count >= 1 || max_count <= 3);

	uint64_t count;

	if (is_nonempty(pieces))
		count = 1;
	else
		count = 0;

	if (max_count > 1) {
		pieces = reset_lsb(pieces);
		if (is_nonempty(pieces))
			++count;
	}

	if (max_count > 2) {
		pieces = reset_lsb(pieces);
		if (is_nonempty(pieces))
			++count;
	}

	return count;
}

static uint64_t
attack_code(const struct position *pos, uint64_t pieces)
{
	uint64_t code;

	code = count_code(pieces & pos->map[opponent_king], 1);

	code *= 2;
	code += count_code(pieces & pos->map[opponent_queen], 1);

	code *= 3;
	code += count_code(pieces & pos->map[opponent_rook], 2);

	code *= 4;
	code += count_code(pieces & pos->nb[1], 3);

	code *= 3;
	code += popcnt(pieces & pos->map[opponent_pawn]);

	code *= 2;
	code += count_code(pieces & pos->map[king], 1);

	code *= 2;
	code += count_code(pieces & pos->map[queen], 1);

	code *= 3;
	code += count_code(pieces & pos->map[rook], 2);

	code *= 4;
	code += count_code(pieces & pos->nb[0], 3);

	code *= 3;
	code += popcnt(pieces & pos->map[pawn]);

	return code;
}

static uint64_t
find_attackers(const struct move_square_desc *sq, const struct position *pos)
{
	if (sq->piece == king)
		return EMPTY;

	uint64_t attackers = EMPTY;

	attackers |= pos->all_kings & king_pattern[sq->index];
	attackers |= pos->all_knights & knight_pattern[sq->index];
	attackers |= pos->all_rq & sq->rreach;
	attackers |= pos->all_bq & sq->breach;
	attackers |= pos->map[pawn] & pawn_att_south(sq->index);
	attackers |= pos->map[opponent_pawn] & pawn_att_north(sq->index);

	return attackers;
}

static uint64_t
find_piece_attacks(const struct move_square_desc *sq)
{
	switch (sq->piece) {
	case pawn:
		return pawn_att_north(sq->index);
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

	uint64_t code = attack_code(pos, sq->attackers);
	int xvalue = SEE_values[code] * piece_value[pawn];

	if (xvalue < piece_value[sq->piece])
		sq->SEE_loss = piece_value[sq->piece] - xvalue;
	else
		sq->SEE_loss = 0;
}

static void
describe_source(struct move_desc *desc, const struct position *pos)
{
	int from = mfrom(desc->move);

	desc->src_sq.index = from;
	desc->src_sq.piece = pos->board[from];
	desc->src_sq.rreach = pos->rays[pr_rook][from];
	desc->src_sq.breach = pos->rays[pr_bishop][from];
	desc->src_sq.attacks = find_piece_attacks(&desc->src_sq);
	desc->src_sq.attacks &= pos->occupied;
	desc->src_sq.attackers = find_attackers(&desc->src_sq, pos);
	compute_SEE_loss(&desc->src_sq, pos);
}

static void
describe_destination(struct move_desc *desc, const struct position *pos)
{
	int to = mto(desc->move);
	uint64_t to64 = mto64(desc->move);

	desc->dst_sq.index = to;
	desc->dst_sq.piece = mresultp(desc->move);
	desc->dst_sq.rreach = pos->rays[pr_rook][to];
	desc->dst_sq.breach = pos->rays[pr_bishop][to];

	if (is_nonempty(desc->src_sq.rreach & to64))
		desc->dst_sq.rreach |= desc->src_sq.rreach & rook_masks[to];
	else if (is_nonempty(desc->src_sq.breach & to64))
		desc->dst_sq.breach |= desc->src_sq.breach & bishop_masks[to];

	desc->dst_sq.attacks = find_piece_attacks(&desc->dst_sq);
	desc->dst_sq.attacks &= pos->occupied & ~mfrom64(desc->move);
	desc->dst_sq.attackers = find_attackers(&desc->dst_sq, pos);
	desc->dst_sq.attackers &= ~mfrom64(desc->move);
	compute_SEE_loss(&desc->dst_sq, pos);
}

static void
find_discovered_attacks(struct move_desc *desc, const struct position *pos)
{
	uint64_t discovered = EMPTY;
	uint64_t reach = desc->src_sq.breach & ~desc->dst_sq.breach;

	for (uint64_t b = reach & pos->bq[0]; is_nonempty(b); b = reset_lsb(b))
		discovered |= reach & bishop_masks[bsf(b)] & pos->map[1];

	for (uint64_t b = reach & pos->bq[1]; is_nonempty(b); b = reset_lsb(b))
		discovered |= reach & bishop_masks[bsf(b)] & pos->map[0];

	reach = desc->src_sq.rreach & ~desc->dst_sq.rreach;

	for (uint64_t r = reach & pos->rq[0]; is_nonempty(r); r = reset_lsb(r))
		discovered |= reach & rook_masks[bsf(r)] & pos->map[1];

	for (uint64_t r = reach & pos->rq[1]; is_nonempty(r); r = reset_lsb(r))
		discovered |= reach & rook_masks[bsf(r)] & pos->map[0];

	desc->discovered_attacks = popcnt(discovered);
	discovered &= pos->map[opponent_king];
	desc->discovered_check = is_nonempty(discovered);
}

static void
find_attacks(struct move_desc *desc, const struct position *pos)
{
	uint64_t attackable = pos->map[1];
	attackable |= pos->map[0] & pos->defendable_hanging[0];

	if (mresultp(desc->move) != pawn) {
		attackable &= ~(pos->map[pawn] & pos->attack[pawn]);
		attackable &= ~(pos->map[pawn + 1] & pos->attack[pawn + 1]);
	}

	desc->attack_count_delta = popcnt(desc->dst_sq.attacks & attackable);
	desc->attack_count_delta -= popcnt(desc->src_sq.attacks & attackable);
	desc->direct_check = is_nonempty(desc->dst_sq.attacks &
					 pos->map[opponent_king]);
	find_discovered_attacks(desc, pos);
}

static void
compute_SEE_value(struct move_desc *desc)
{
	desc->SEE_value = piece_value[mcapturedp(desc->move)];
	desc->SEE_value -= piece_value[desc->src_sq.piece];
	desc->SEE_value += piece_value[desc->dst_sq.piece];
	desc->SEE_value += desc->src_sq.SEE_loss;
	desc->SEE_value -= desc->dst_sq.SEE_loss;
}

static uint16_t
new_direct_attacks_value(const struct move_desc *desc,
			 const struct position *pos)
{
	uint16_t value = 0;

	uint64_t attacks = desc->dst_sq.attacks & ~desc->src_sq.attacks;
	value += popcnt(attacks);
	attacks &= pos->undefended[1];

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks))
		value += piece_value[pos->board[bsf(attacks)]] / 5;

	return value;
}

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

void
describe_move(struct move_desc *desc, const struct position *pos, move m)
{
	move prev_m = desc->move;

	if (prev_m == m)
		return;

	desc->move = m;

	if (prev_m == 0 || (mfrom(prev_m) != mfrom(m)))
		describe_source(desc, pos);

	describe_destination(desc, pos);
	find_attacks(desc, pos);
	compute_SEE_value(desc);
	desc->value = desc->SEE_value;
	if (desc->dst_sq.SEE_loss == 0 || desc->discovered_check)
		desc->value += new_direct_attacks_value(desc, pos);

	desc->value -= popcnt(desc->discovered_attacks);

	if (desc->direct_check) {
		uint64_t att = desc->discovered_attacks & pos->map[1];
		att &= pos->undefended[1];
		desc->value += popcnt(att) * 100;
		desc->value += popcnt(att & pos->rq[1]) * 100;
		desc->value += 10;
	}

	desc->value += move_dest_table[mto(m)];
}
