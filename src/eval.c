/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "eval_terms.h"
#include "eval.h"

const int piece_value[14] = {
	0, 0,
	pawn_value, -pawn_value,
	rook_value, -rook_value,
	0, 0,
	bishop_value, -bishop_value,
	knight_value, -knight_value,
	queen_value, -queen_value
};

enum {
	rook_of_value = 7,
	rook_hf_value = 18,
	rook_battery_value = 7,

	pawn_on_center_value = 12,
	pawn_on_center4_value = 6,
	center_attack_value = 9,
	knight_attack_center = 4,
	bishop_attack_center = 4,

	isolated_pawn_value = -10,
	blocked_pawn_value = -10,
	double_pawn_value = -10,
	backward_pawn_value = -10,
	pawn_chain_value = 3,

	knight_outpost_value = 24,
	knight_outpost_reach_value = 10,
	knight_on_edge_value = -8,

	bishop_pair_value = 3,
	bishop_wrong_color_value = -1,

	kings_pawn_value = 24,
	castled_king_value = 30,
	king_ring_sliding_attacked_value = -11,
	castle_right_value = 10,
	king_open_file_value = -26,
	king_pawn_storm_value = -13,

	passed_pawn_value = 32,

	bishop_trapped_value = -16,
	bishop_trapped_by_opponent_value = -60,
	rook_trapped_value = -32,
	knight_cornered_value = -16,

	threat_value = 20
};

enum {
	king_safety_treshold = queen_value + knight_value
};

static int
eval_basic_mobility(const struct position *pos)
{
	int value = 0;

	value += popcnt(pos->attack[bishop]);
	value += popcnt(pos->attack[knight]);
	value += popcnt(pos->attack[rook]);
	value += popcnt(pos->attack[queen]) / 2;
	value -= popcnt(pos->attack[opponent_bishop]);
	value -= popcnt(pos->attack[opponent_knight]);
	value -= popcnt(pos->attack[opponent_rook]);
	value -= popcnt(pos->attack[opponent_queen]) / 2;

	if (non_pawn_material(pos) > rook_value * 3) {
		uint64_t other_side = RANK_6 | RANK_7 | RANK_8;
		value += 6 * popcnt(pos->attack[pawn] & other_side);
		value += 3 * popcnt(pos->attack[bishop] & other_side);
		value += 3 * popcnt(pos->attack[knight] & other_side);
		value += 3 * popcnt(pos->attack[rook] & other_side);
	}

	if (opponent_non_pawn_material(pos) > rook_value * 3) {
		uint64_t other_side = RANK_3 | RANK_2 | RANK_1;
		value -= 6 * popcnt(pos->attack[opponent_pawn] & other_side);
		value -= 3 * popcnt(pos->attack[opponent_bishop] & other_side);
		value -= 3 * popcnt(pos->attack[opponent_knight] & other_side);
		value -= 3 * popcnt(pos->attack[opponent_rook] & other_side);
	}

	uint64_t free_sq = free_squares(pos);

	value += popcnt(free_sq);
	value += popcnt(pos->attack[bishop] & free_sq);
	value += popcnt(pos->attack[knight] & free_sq);
	value += popcnt(pos->attack[rook] & free_sq);

	free_sq &= CENTER4_SQ;
	value += popcnt(free_sq);
	value += 4 * popcnt(pos->attack[bishop] & free_sq);
	value += 4 * popcnt(pos->attack[knight] & free_sq);

	free_sq = opponent_free_squares(pos);

	value -= popcnt(free_sq);
	value -= popcnt(pos->attack[opponent_bishop] & free_sq);
	value -= popcnt(pos->attack[opponent_knight] & free_sq);
	value -= popcnt(pos->attack[opponent_rook] & free_sq);

	free_sq &= CENTER4_SQ;
	value -= popcnt(free_sq);
	value -= 4 * popcnt(pos->attack[opponent_bishop] & free_sq);
	value -= 4 * popcnt(pos->attack[opponent_knight] & free_sq);

	return (value * (popcnt(pos->occupied) + 6)) / 20;
}

static int
king_safety_side(uint64_t king_map, uint64_t pawns, uint64_t opp_pawns,
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
		value += castled_king_value;
	else if (is_nonempty(king_map & (SQ_A1|SQ_C1|SQ_B1))
	    && is_empty(rooks & SQ_A1))
		value += castled_king_value;

	/* Count pawns around the king */
	uint64_t p = king_reach & pawns;
	value += (kings_pawn_value / 2) * popcnt(p & ~opp_attacks);

	p = (king_reach | north_of(king_reach)) & pawn_attacks_player(pawns);
	value += kings_pawn_value * popcnt(p);

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
	value += king_open_file_value * popcnt(shield & half_open_files);

	// More Penalty for the king itself residing on an open file
	if (is_nonempty(king_map & half_open_files))
		value += king_open_file_value;

	// Penalty for opponent pawns advancing towards the king
	uint64_t storm = kogge_stone_north(shield) & opp_pawns;
	storm &= RANK_2 | RANK_3 | RANK_4 | RANK_5;
	value += king_pawn_storm_value * popcnt(storm);
	value += king_pawn_storm_value * popcnt(storm & ~RANK_5);
	value += king_pawn_storm_value * popcnt(storm & opp_sattacks & ~RANK_5);
	value += king_pawn_storm_value * popcnt(storm & north_of(shield));

	return value;
}

static int
king_safety_wrapper(const struct position *pos)
{
	if (is_empty(pos->map[king] & (RANK_1|RANK_2)))
		return -50;

	return king_safety_side(pos->map[king],
	    pos->map[pawn],
	    pos->map[opponent_pawn],
	    pos->half_open_files[0],
	    pos->map[rook],
	    pos->attack[king],
	    pos->attack[1],
	    pos->sliding_attacks[1]);
}

static int
opponent_king_safety_wrapper(const struct position *pos)
{
	if (is_empty(pos->map[opponent_king] & (RANK_8|RANK_7)))
		return -50;

	return king_safety_side(bswap(pos->map[opponent_king]),
	    bswap(pos->map[opponent_pawn]),
	    bswap(pos->map[pawn]),
	    bswap(pos->half_open_files[1]),
	    bswap(pos->map[opponent_rook]),
	    bswap(pos->attack[opponent_king]),
	    bswap(pos->attack[0]),
	    bswap(pos->sliding_attacks[0]));
}

static int
eval_king_safety(const struct position *pos)
{
	int value = 0;

	if (opponent_non_pawn_material(pos) > king_safety_treshold)
		value += (king_safety_wrapper(pos)
		    * opponent_non_pawn_material(pos))
		    / (30 * pawn_value);

	if (non_pawn_material(pos) > king_safety_treshold)
		value -= (opponent_king_safety_wrapper(pos)
		    * non_pawn_material(pos))
		    / (30 * pawn_value);

	if (pos->cr_king_side || pos->cr_queen_side)
		value += castle_right_value;

	if (pos->cr_opponent_king_side || pos->cr_opponent_queen_side)
		value -= castle_right_value;

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

	value *= non_pawn_material(pos) + opponent_non_pawn_material(pos);
	value /= 60 * pawn_value;

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

	if (rook_a1_is_trapped(pos))
		value += rook_trapped_value;

	if (rook_h1_is_trapped(pos))
		value += rook_trapped_value;

	if (opponent_rook_a8_is_trapped(pos))
		value -= rook_trapped_value;

	if (opponent_rook_h8_is_trapped(pos))
		value -= rook_trapped_value;


	return value;
}

static int
filecnt(uint64_t bitboard)
{
	return popcnt(kogge_stone_north(bitboard) & RANK_8);
}

static int
eval_pawn_structure(const struct position *pos)
{
	int value = 0;

	value += pawn_chain_value * popcnt(pawn_chains(pos));
	value -= pawn_chain_value * popcnt(opponent_pawn_chains(pos));

	value += isolated_pawn_value * filecnt(isolated_pawns(pos));
	value -= isolated_pawn_value * filecnt(opponent_isolated_pawns(pos));
	value += blocked_pawn_value * filecnt(blocked_pawns(pos));
	value -= blocked_pawn_value * filecnt(opponent_blocked_pawns(pos));
	value += double_pawn_value * popcnt(double_pawns(pos));
	value -= double_pawn_value * popcnt(opponent_double_pawns(pos));
	value += backward_pawn_value * filecnt(backward_pawns(pos));
	value -= backward_pawn_value * filecnt(opponent_backward_pawns(pos));

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

	value += knight_on_edge_value
	    * popcnt(pos->map[knight] & EDGES);
	value -= knight_on_edge_value
	    * popcnt(pos->map[opponent_knight] & EDGES);

	if (knight_cornered_a8(pos))
		value += knight_cornered_value;

	if (knight_cornered_h8(pos))
		value += knight_cornered_value;

	if (opponent_knight_cornered_a1(pos))
		value -= knight_cornered_value;

	if (opponent_knight_cornered_h1(pos))
		value -= knight_cornered_value;

	return value;
}

static int
eval_bishop_placement(const struct position *pos)
{
	int value = 0;

	if (has_bishop_pair(pos))
		value += bishop_pair_value * (16 - popcnt(all_pawns(pos)));

	if (opponent_has_bishop_pair(pos))
		value -= bishop_pair_value * (16 - popcnt(all_pawns(pos)));

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

	if (bishop_c1_is_trapped(pos))
		value += bishop_trapped_value;

	if (bishop_f1_is_trapped(pos))
		value += bishop_trapped_value;

	if (opponent_bishop_c8_is_trapped(pos))
		value -= bishop_trapped_value;

	if (opponent_bishop_f8_is_trapped(pos))
		value -= bishop_trapped_value;

	if (bishop_trapped_at_a7(pos))
		value += bishop_trapped_by_opponent_value;
	else if (bishop_trappable_at_a7(pos))
		value += bishop_trapped_by_opponent_value / 2;

	if (bishop_trapped_at_h7(pos))
		value += bishop_trapped_by_opponent_value;
	else if (bishop_trappable_at_h7(pos))
		value += bishop_trapped_by_opponent_value / 2;

	if (opponent_bishop_trapped_at_a2(pos))
		value -= bishop_trapped_by_opponent_value;
	else if (opponent_bishop_trappable_at_a2(pos))
		value -= bishop_trapped_by_opponent_value / 2;

	if (opponent_bishop_trapped_at_h2(pos))
		value -= bishop_trapped_by_opponent_value;
	else if (opponent_bishop_trappable_at_h2(pos))
		value -= bishop_trapped_by_opponent_value / 2;

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
	value += (passed_pawn_value / 2) * popcnt(pawns & RANK_6);
	value -= (passed_pawn_value / 2)* popcnt(opp_pawns & RANK_3);
	value += passed_pawn_value * popcnt(pawns & RANK_7);
	value -= passed_pawn_value * popcnt(opp_pawns & RANK_2);

	pawns &= pos->attack[pawn];
	opp_pawns &= pos->attack[opponent_pawn];

	value += (passed_pawn_value / 3) * popcnt(pawns & RANK_6);
	value -= (passed_pawn_value / 3) * popcnt(opp_pawns & RANK_3);
	value += (passed_pawn_value / 2) * popcnt(pawns & RANK_7);
	value -= (passed_pawn_value / 2) * popcnt(opp_pawns & RANK_2);

	return value;
}

int
eval_threats(const struct position *pos)
{
	int value = 0;

	uint64_t attacks = pos->attack[pawn];
	uint64_t victims = pos->map[1] & ~pos->map[opponent_pawn];
	value += threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[knight] & pos->attack[bishop];
	victims = pos->map[opponent_rook] | pos->map[opponent_queen];
	value += threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[rook];
	victims = pos->map[opponent_queen];
	value += threat_value * popcnt(attacks & victims);

	attacks = pos->attack[opponent_pawn];
	victims = pos->map[0] & ~pos->map[pawn];
	value -= threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[opponent_knight] & pos->attack[opponent_bishop];
	victims = pos->map[rook] | pos->map[queen];
	value -= threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[opponent_rook];
	victims = pos->map[queen];
	value -= threat_value * popcnt(attacks & victims);

	victims = pos->map[1] & ~pos->attack[1];
	attacks = pos->attack[0];
	value += threat_value * popcnt(attacks & victims);

	victims = pos->map[0] & ~pos->attack[0];
	attacks = pos->attack[1];
	value -= threat_value * popcnt(attacks & victims);

	return value;
}

int
eval(const struct position *pos)
{
	int value = 0;

	value += pos->material_value;
	value -= pos->opponent_material_value;
	value += eval_basic_mobility(pos);
	value += eval_passed_pawns(pos);
	value += eval_pawn_structure(pos);
	value += eval_knight_placement(pos);
	value += eval_threats(pos);
	if (is_nonempty(pos->sliding_attacks[0] | pos->sliding_attacks[1])) {
		value += eval_rook_placement(pos);
		value += eval_bishop_placement(pos);
		value += eval_king_safety(pos);
		value += eval_center_control(pos);
	}

	invariant(value_bounds(value));

	return value;
}


struct eval_factors
compute_eval_factors(const struct position *pos)
{
	struct eval_factors ef;

	ef.material = pos->material_value - pos->opponent_material_value;
	ef.basic_mobility = eval_basic_mobility(pos);
	ef.center_control = eval_center_control(pos);
	ef.pawn_structure = eval_pawn_structure(pos);
	ef.passed_pawns = eval_passed_pawns(pos);
	ef.king_safety = eval_king_safety(pos);
	ef.rook_placement = eval_rook_placement(pos);
	ef.knight_placement = eval_knight_placement(pos);
	ef.bishop_placement = eval_bishop_placement(pos);
	ef.threats = eval_threats(pos);
	return ef;
}
