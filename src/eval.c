
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "eval_terms.h"
#include "eval.h"

const int piece_value[14] = {
	0, 0,
	pawn_value, -pawn_value,
	rook_value, -rook_value,
	mate_value, -mate_value,
	bishop_value, -bishop_value,
	knight_value, -knight_value,
	queen_value, -queen_value
};

enum {
	rook_of_value = 7,
	rook_hf_value = 18,
	rook_battery_value = 7,
	pawn_on_center_value = 12,
	pawn_on_center4_value = 10,
	isolated_pawn_value = -13,
	blocked_pawn_value = -8,
	double_pawn_value = -17,
	backward_pawn_value = -24,
	center_attack_value = 3,
	pawn_chain_value = 3,
	knight_attack_center = 4,
	bishop_attack_center = 4,
	knight_outpost_value = 30,
	knight_outpost_reach_value = 17,
	kings_pawn_value = 5,
	catled_king_value = 5,
	king_ring_sliding_attacked_value = -4,
	passed_pawn_value = 15,
	bishop_pair_value = 2,
	bishop_wrong_color_value = -3,
	tempo = 3
};

static int
eval_basic_mobility(const struct position *pos)
{
	int value = 0;

	value += popcnt(squares_reachable_bnrq(pos));
	value -= popcnt(opponent_squares_reachable_bnrq(pos));
	value += popcnt(free_squares_reachable_bnrq(pos));
	value -= popcnt(opponent_free_squares_reachable_bnrq(pos));

	return value / 2;
}

static int
end_game(const struct position *pos)
{
	return 3 - ((popcnt(pos->occupied) - 1) / 8);
}

static int
middle_game(const struct position *pos)
{
	if (popcnt(pos->occupied) > 15) {
		int value = (popcnt(pos->occupied) - 1) / 6;
		value += popcnt(pos->map[queen] | pos->map[opponent_queen]);
		return value / 3;
	}
	else return 0;
}

static int
king_fortress_side(uint64_t king_map, uint64_t pawns,
		uint64_t half_open_files,
		uint64_t rooks, uint64_t king_reach,
		uint64_t opp_attacks, uint64_t opp_sattacks)
{
	int value = 0;

	/*
	 * Bonus for castled king to encourage castling,
	 * or at least not losing castling rights.
	 */
	if (is_nonempty(king_map & (SQ_G1|SQ_H1))
	    && is_empty(rooks & SQ_H1))
		value += catled_king_value;
	else if (is_nonempty(king_map & (SQ_A1|SQ_C1|SQ_B1))
	    && is_empty(rooks & SQ_A1))
		value += catled_king_value;

	/* Count pawns around the king */
	uint64_t p = king_reach & pawns;
	value += (kings_pawn_value / 2) * popcnt(p);
	value += kings_pawn_value * popcnt(p & ~opp_attacks);

	/*
	 * Penalty for squares around the king being attacked by
	 * other player
	 */
	value += king_ring_sliding_attacked_value *
	    popcnt(king_reach & opp_sattacks);

	/* shield - up to three squares in front of the king */
	uint64_t shield = north_of(king_map)
	    | north_of(west_of(king_map & ~FILE_H))
	    | north_of(east_of(king_map & ~FILE_A));

	// Penalty for not having any pawn on a file around the king
	value -= popcnt(shield & half_open_files);

	/*
	 * Even more bonus for having a pawn in front of the king,
	 * vs. just left or right to the king.
	 */
	p = north_of(shield) & pawns;
	value += kings_pawn_value * popcnt(p);

	return value;
}

static int
eval_king_fortress(const struct position *pos)
{
	int value = 0;

	if (is_nonempty(pos->map[king] & (RANK_1|RANK_2)))
		value += king_fortress_side(pos->map[king],
		    pos->map[pawn],
		    pos->half_open_files[0],
		    pos->map[rook],
		    pos->attack[king],
		    pos->attack[1],
		    pos->sliding_attacks[1]);

	if (is_nonempty(pos->map[opponent_king] & (RANK_8|RANK_7)))
		value -= king_fortress_side(bswap(pos->map[opponent_king]),
		    bswap(pos->map[opponent_pawn]),
		    bswap(pos->half_open_files[1]),
		    bswap(pos->map[opponent_rook]),
		    bswap(pos->attack[opponent_king]),
		    bswap(pos->attack[0]),
		    bswap(pos->sliding_attacks[0]));

	value += pos->cr_king_side + pos->cr_queen_side;
	value -= pos->cr_opponent_king_side + pos->cr_opponent_queen_side;

	return value;
}

static int
eval_center_control(const struct position *pos)
{
	int value = 0;

	value += pawn_on_center_value * popcnt(pawns_on_center(pos));
	value -= pawn_on_center_value * popcnt(opponent_pawns_on_center(pos));
	value += pawn_on_center4_value * popcnt(pawns_on_center4(pos));
	value -= pawn_on_center4_value * popcnt(opponent_pawns_on_center4(pos));
	value += center_attack_value * popcnt(center4_attacks(pos));
	value -= center_attack_value * popcnt(opponent_center4_attacks(pos));

	return value;
}

static int
eval_rook_placement(const struct position *pos)
{
	int value = 0;

	value += rook_hf_value * popcnt(rooks_on_half_open_files(pos));
	value -= rook_hf_value * popcnt(opponent_rooks_on_half_open_files(pos));
	value += rook_of_value * popcnt(rooks_on_open_files(pos));
	value -= rook_of_value * popcnt(opponent_rooks_on_open_files(pos));
	value += rook_battery_value * popcnt(rook_batteries(pos));
	value -= rook_battery_value * popcnt(opponent_rook_batteries(pos));

	return value;
}

static int
eval_pawn_structure(const struct position *pos)
{
	int value = 0;

	value += pawn_chain_value * popcnt(pawn_chains(pos));
	value -= pawn_chain_value * popcnt(opponent_pawn_chains(pos));
	value += isolated_pawn_value * popcnt(isolated_pawns(pos));
	value -= isolated_pawn_value * popcnt(opponent_isolated_pawns(pos));
	value += blocked_pawn_value * popcnt(blocked_pawns(pos));
	value -= blocked_pawn_value * popcnt(opponent_blocked_pawns(pos));
	value += double_pawn_value * popcnt(double_pawns(pos));
	value -= double_pawn_value * popcnt(opponent_double_pawns(pos));
	value += backward_pawn_value * popcnt(backward_pawns(pos));
	value -= backward_pawn_value * popcnt(opponent_backward_pawns(pos));

	return value;
}

static int
eval_knight_placement(const struct position *pos)
{
	int value = 0;

	value += knight_outpost_value * popcnt(knight_outposts(pos));
	value -= knight_outpost_value * popcnt(opponent_knight_outposts(pos));
	value += knight_outpost_reach_value
	    * popcnt(knight_reach_outposts(pos));
	value -= knight_outpost_reach_value
	    * popcnt(opponent_knight_reach_outposts(pos));

	return value;
}

static int
eval_bishop_placement(const struct position *pos)
{
	int value = 0;

	if (has_bishop_pair(pos))
		value += bishop_pair_value * (17 - popcnt(all_pawns(pos)));

	if (opponent_has_bishop_pair(pos))
		value -= bishop_pair_value * (17 - popcnt(all_pawns(pos)));

	value += bishop_wrong_color_value
	    * popcnt(bishops_on_white(pos)) * popcnt(pawns_on_white(pos));
	value -= bishop_wrong_color_value
	    * popcnt(opponent_bishops_on_white(pos))
	    * popcnt(pawns_on_white(pos));
	value += bishop_wrong_color_value
	    * popcnt(bishops_on_black(pos)) * popcnt(pawns_on_black(pos));
	value -= bishop_wrong_color_value
	    * popcnt(opponent_bishops_on_black(pos))
	    * popcnt(pawns_on_black(pos));

	return value;
}

static int
eval_passed_pawns(const struct position *pos)
{
	int value = 0;
	uint64_t pawns = passed_pawns(pos);
	uint64_t opp_pawns = opponent_passed_pawns(pos);

	value += passed_pawn_value * popcnt(pawns);
	value -= passed_pawn_value * popcnt(opp_pawns);
	value += passed_pawn_value * popcnt(pawns & RANK_6);
	value -= passed_pawn_value * popcnt(opp_pawns & RANK_3);
	value += 2 * passed_pawn_value * popcnt(pawns & RANK_7);
	value -= 2 * passed_pawn_value * popcnt(opp_pawns & RANK_2);

	pawns &= pos->attack[pawn];
	opp_pawns &= pos->attack[opponent_pawn];

	value += passed_pawn_value * popcnt(pawns);
	value -= passed_pawn_value * popcnt(opp_pawns);
	value += passed_pawn_value * popcnt(pawns & RANK_6);
	value -= passed_pawn_value * popcnt(opp_pawns & RANK_3);
	value += 2 * passed_pawn_value * popcnt(pawns & RANK_7);
	value -= 2 * passed_pawn_value * popcnt(opp_pawns & RANK_2);


	return value;
}

int
eval(const struct position *pos)
{
	int value;

	value = pos->material_value;
	value += eval_basic_mobility(pos);
	value += eval_rook_placement(pos);
	value += eval_pawn_structure(pos);
	value += eval_knight_placement(pos);
	value += eval_bishop_placement(pos);
	int mg_factor = middle_game(pos);
	if (mg_factor != 0)
		value += mg_factor *
		    (eval_center_control(pos) + eval_king_fortress(pos));
	int eg_factor = end_game(pos);
	value += (eg_factor + 1) * eval_passed_pawns(pos);

	invariant(value_bounds(value));

	return value + tempo;
}


struct eval_factors
compute_eval_factors(const struct position *pos)
{
	struct eval_factors ef;

	ef.material = pos->material_value;
	ef.basic_mobility = eval_basic_mobility(pos);
	ef.end_game = end_game(pos);
	ef.middle_game = middle_game(pos);
	ef.center_control = eval_center_control(pos);
	ef.pawn_structure = eval_pawn_structure(pos);
	ef.passed_pawns = eval_passed_pawns(pos);
	ef.king_fortress = eval_king_fortress(pos);
	ef.rook_placement = eval_rook_placement(pos);
	ef.knight_placement = eval_knight_placement(pos);
	ef.bishop_placement = eval_bishop_placement(pos);
	ef.center_control = eval_center_control(pos);
	return ef;
}
