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
	bishop_trapped_by_black_value = -60,
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

	value += popcnt(pos->attack[white_bishop]);
	value += popcnt(pos->attack[white_knight]);
	value += popcnt(pos->attack[white_rook]);
	value += popcnt(pos->attack[white_queen]) / 2;
	value -= popcnt(pos->attack[black_bishop]);
	value -= popcnt(pos->attack[black_knight]);
	value -= popcnt(pos->attack[black_rook]);
	value -= popcnt(pos->attack[black_queen]) / 2;

	if (white_non_pawn_material(pos) > rook_value * 3) {
		uint64_t other_side = RANK_6 | RANK_7 | RANK_8;
		value += 6 * popcnt(pos->attack[white_pawn] & other_side);
		value += 3 * popcnt(pos->attack[white_bishop] & other_side);
		value += 3 * popcnt(pos->attack[white_knight] & other_side);
		value += 3 * popcnt(pos->attack[white_rook] & other_side);
	}

	if (black_non_pawn_material(pos) > rook_value * 3) {
		uint64_t other_side = RANK_3 | RANK_2 | RANK_1;
		value -= 6 * popcnt(pos->attack[black_pawn] & other_side);
		value -= 3 * popcnt(pos->attack[black_bishop] & other_side);
		value -= 3 * popcnt(pos->attack[black_knight] & other_side);
		value -= 3 * popcnt(pos->attack[black_rook] & other_side);
	}

	uint64_t free_sq = white_free_squares(pos);

	value += popcnt(free_sq);
	value += popcnt(pos->attack[white_bishop] & free_sq);
	value += popcnt(pos->attack[white_knight] & free_sq);
	value += popcnt(pos->attack[white_rook] & free_sq);

	free_sq &= CENTER4_SQ;
	value += popcnt(free_sq);
	value += 4 * popcnt(pos->attack[white_bishop] & free_sq);
	value += 4 * popcnt(pos->attack[white_knight] & free_sq);

	free_sq = black_free_squares(pos);

	value -= popcnt(free_sq);
	value -= popcnt(pos->attack[black_bishop] & free_sq);
	value -= popcnt(pos->attack[black_knight] & free_sq);
	value -= popcnt(pos->attack[black_rook] & free_sq);

	free_sq &= CENTER4_SQ;
	value -= popcnt(free_sq);
	value -= 4 * popcnt(pos->attack[black_bishop] & free_sq);
	value -= 4 * popcnt(pos->attack[black_knight] & free_sq);

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

	p = (king_reach | north_of(king_reach)) & white_pawn_attacks(pawns);
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
white_king_safety_wrapper(const struct position *pos)
{
	if (is_empty(pos->map[white_king] & (RANK_1|RANK_2)))
		return -50;

	return king_safety_side(pos->map[white_king],
	    pos->map[white_pawn],
	    pos->map[black_pawn],
	    pos->half_open_files[white],
	    pos->map[white_rook],
	    pos->attack[white_king],
	    pos->attack[black],
	    pos->sliding_attacks[black]);
}

static int
black_king_safety_wrapper(const struct position *pos)
{
	if (is_empty(pos->map[black_king] & (RANK_8|RANK_7)))
		return -50;

	return king_safety_side(bswap(pos->map[black_king]),
	    bswap(pos->map[black_pawn]),
	    bswap(pos->map[white_pawn]),
	    bswap(pos->half_open_files[black]),
	    bswap(pos->map[black_rook]),
	    bswap(pos->attack[black_king]),
	    bswap(pos->attack[white]),
	    bswap(pos->sliding_attacks[white]));
}

static int
eval_king_safety(const struct position *pos)
{
	int value = 0;

	if (black_non_pawn_material(pos) > king_safety_treshold)
		value += (white_king_safety_wrapper(pos)
		    * black_non_pawn_material(pos))
		    / (30 * pawn_value);

	if (white_non_pawn_material(pos) > king_safety_treshold)
		value -= (black_king_safety_wrapper(pos)
		    * white_non_pawn_material(pos))
		    / (30 * pawn_value);

	if (pos->cr_white_king_side || pos->cr_white_queen_side)
		value += castle_right_value;

	if (pos->cr_black_king_side || pos->cr_black_queen_side)
		value -= castle_right_value;

	return value;
}

static int
eval_center_control(const struct position *pos)
{
	int value = 0;

	value += pawn_on_center_value * popcnt(white_pawns_on_center(pos));
	value -= pawn_on_center_value * popcnt(black_pawns_on_center(pos));
	value += pawn_on_center4_value * popcnt(white_pawns_on_center4(pos));
	value -= pawn_on_center4_value * popcnt(black_pawns_on_center4(pos));
	value += center_attack_value * popcnt(white_center4_attacks(pos));
	value -= center_attack_value * popcnt(black_center4_attacks(pos));

	value *= white_non_pawn_material(pos) + black_non_pawn_material(pos);
	value /= 60 * pawn_value;

	return value;
}

static int
eval_rook_placement(const struct position *pos)
{
	int value = 0;

	value += rook_hf_value * popcnt(white_rooks_on_half_open_files(pos));
	value -= rook_hf_value * popcnt(black_rooks_on_half_open_files(pos));
	value += rook_of_value * popcnt(white_rooks_on_open_files(pos));
	value -= rook_of_value * popcnt(black_rooks_on_open_files(pos));
	value += rook_battery_value * popcnt(white_rook_batteries(pos));
	value -= rook_battery_value * popcnt(black_rook_batteries(pos));

	if (rook_a1_is_trapped(pos))
		value += rook_trapped_value;

	if (rook_h1_is_trapped(pos))
		value += rook_trapped_value;

	if (rook_a8_is_trapped(pos))
		value -= rook_trapped_value;

	if (rook_h8_is_trapped(pos))
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

	value += pawn_chain_value * popcnt(white_pawn_chains(pos));
	value -= pawn_chain_value * popcnt(black_pawn_chains(pos));

	value += isolated_pawn_value * filecnt(white_isolated_pawns(pos));
	value -= isolated_pawn_value * filecnt(black_isolated_pawns(pos));
	value += blocked_pawn_value * filecnt(white_blocked_pawns(pos));
	value -= blocked_pawn_value * filecnt(black_blocked_pawns(pos));
	value += double_pawn_value * popcnt(white_double_pawns(pos));
	value -= double_pawn_value * popcnt(black_double_pawns(pos));
	value += backward_pawn_value * filecnt(white_backward_pawns(pos));
	value -= backward_pawn_value * filecnt(black_backward_pawns(pos));

	return value;
}

static int
eval_knight_placement(const struct position *pos)
{
	int value = 0;

	value += knight_outpost_value * popcnt(white_knight_outposts(pos));
	value -= knight_outpost_value * popcnt(black_knight_outposts(pos));
	value += knight_outpost_reach_value
	    * popcnt(white_knight_reach_outposts(pos));
	value -= knight_outpost_reach_value
	    * popcnt(black_knight_reach_outposts(pos));

	value += knight_on_edge_value
	    * popcnt(pos->map[white_knight] & EDGES);
	value -= knight_on_edge_value
	    * popcnt(pos->map[black_knight] & EDGES);

	if (white_knight_cornered_a8(pos))
		value += knight_cornered_value;

	if (white_knight_cornered_h8(pos))
		value += knight_cornered_value;

	if (black_knight_cornered_a1(pos))
		value -= knight_cornered_value;

	if (black_knight_cornered_h1(pos))
		value -= knight_cornered_value;

	return value;
}

static int
eval_bishop_placement(const struct position *pos)
{
	int value = 0;

	if (white_has_bishop_pair(pos))
		value += bishop_pair_value * (16 - popcnt(all_pawns(pos)));

	if (black_has_bishop_pair(pos))
		value -= bishop_pair_value * (16 - popcnt(all_pawns(pos)));

	value += bishop_wrong_color_value
	    * popcnt(white_bishops_on_white(pos)) * popcnt(pawns_on_white(pos));
	value -= bishop_wrong_color_value
	    * popcnt(black_bishops_on_white(pos))
	    * popcnt(pawns_on_white(pos));
	value += bishop_wrong_color_value
	    * popcnt(white_bishops_on_black(pos)) * popcnt(pawns_on_black(pos));
	value -= bishop_wrong_color_value
	    * popcnt(black_bishops_on_black(pos))
	    * popcnt(pawns_on_black(pos));

	if (bishop_c1_is_trapped(pos))
		value += bishop_trapped_value;

	if (bishop_f1_is_trapped(pos))
		value += bishop_trapped_value;

	if (bishop_c8_is_trapped(pos))
		value -= bishop_trapped_value;

	if (bishop_f8_is_trapped(pos))
		value -= bishop_trapped_value;

	if (white_bishop_trapped_at_a7(pos))
		value += bishop_trapped_by_black_value;
	else if (white_bishop_trappable_at_a7(pos))
		value += bishop_trapped_by_black_value / 2;

	if (white_bishop_trapped_at_h7(pos))
		value += bishop_trapped_by_black_value;
	else if (white_bishop_trappable_at_h7(pos))
		value += bishop_trapped_by_black_value / 2;

	if (black_bishop_trapped_at_a2(pos))
		value -= bishop_trapped_by_black_value;
	else if (black_bishop_trappable_at_a2(pos))
		value -= bishop_trapped_by_black_value / 2;

	if (black_bishop_trapped_at_h2(pos))
		value -= bishop_trapped_by_black_value;
	else if (black_bishop_trappable_at_h2(pos))
		value -= bishop_trapped_by_black_value / 2;

	return value;
}

static int
eval_passed_pawns(const struct position *pos)
{
	int value = 0;
	uint64_t pawns = white_passed_pawns(pos);
	uint64_t opp_pawns = black_passed_pawns(pos);

	value += passed_pawn_value * popcnt(pawns);
	value -= passed_pawn_value * popcnt(opp_pawns);
	value += (passed_pawn_value / 2) * popcnt(pawns & RANK_6);
	value -= (passed_pawn_value / 2)* popcnt(opp_pawns & RANK_3);
	value += passed_pawn_value * popcnt(pawns & RANK_7);
	value -= passed_pawn_value * popcnt(opp_pawns & RANK_2);

	pawns &= pos->attack[white_pawn];
	opp_pawns &= pos->attack[black_pawn];

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

	uint64_t attacks = pos->attack[white_pawn];
	uint64_t victims = pos->map[black] & ~pos->map[black_pawn];
	value += threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[white_knight] & pos->attack[white_bishop];
	victims = pos->map[black_rook] | pos->map[black_queen];
	value += threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[white_rook];
	victims = pos->map[black_queen];
	value += threat_value * popcnt(attacks & victims);

	attacks = pos->attack[black_pawn];
	victims = pos->map[white] & ~pos->map[white_pawn];
	value -= threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[black_knight] & pos->attack[black_bishop];
	victims = pos->map[white_rook] | pos->map[white_queen];
	value -= threat_value * popcnt(attacks & victims);
	attacks |= pos->attack[black_rook];
	victims = pos->map[white_queen];
	value -= threat_value * popcnt(attacks & victims);

	victims = pos->map[black] & ~pos->attack[black];
	attacks = pos->attack[white];
	value += threat_value * popcnt(attacks & victims);

	victims = pos->map[white] & ~pos->attack[white];
	attacks = pos->attack[black];
	value -= threat_value * popcnt(attacks & victims);

	return value;
}

int
eval(const struct position *pos)
{
	int value = 0;

	value += pos->material_value[white];
	value -= pos->material_value[black];
	value += eval_basic_mobility(pos);
	value += eval_passed_pawns(pos);
	value += eval_pawn_structure(pos);
	value += eval_knight_placement(pos);
	value += eval_threats(pos);
	if (is_nonempty(pos->sliding_attacks[white] | pos->sliding_attacks[black])) {
		value += eval_rook_placement(pos);
		value += eval_bishop_placement(pos);
		value += eval_king_safety(pos);
		value += eval_center_control(pos);
	}

	invariant(value_bounds(value));

	if (pos->turn == black)
		value = -value;

	return value;
}


struct eval_factors
compute_eval_factors(const struct position *pos)
{
	struct eval_factors ef;

	ef.material = pos->material_value[white] - pos->material_value[black];
	ef.basic_mobility = eval_basic_mobility(pos);
	ef.center_control = eval_center_control(pos);
	ef.pawn_structure = eval_pawn_structure(pos);
	ef.passed_pawns = eval_passed_pawns(pos);
	ef.king_safety = eval_king_safety(pos);
	ef.rook_placement = eval_rook_placement(pos);
	ef.knight_placement = eval_knight_placement(pos);
	ef.bishop_placement = eval_bishop_placement(pos);
	ef.threats = eval_threats(pos);

	if (pos->turn == black) {
		ef.material = -ef.material;
		ef.basic_mobility = -ef.basic_mobility;
		ef.center_control = -ef.center_control;
		ef.pawn_structure = -ef.pawn_structure;
		ef.passed_pawns = -ef.passed_pawns;
		ef.king_safety = -ef.king_safety;
		ef.rook_placement = -ef.rook_placement;
		ef.knight_placement = -ef.knight_placement;
		ef.bishop_placement = -ef.bishop_placement;
		ef.threats = -ef.threats;
	}

	return ef;
}
