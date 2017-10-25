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

#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "eval.h"

namespace taltos
{

const std::array<int, 14> piece_value = {
	0, 0,
	pawn_value, -pawn_value,
	rook_value, -rook_value,
	0, 0,
	bishop_value, -bishop_value,
	knight_value, -knight_value,
	queen_value, -queen_value
};

constexpr int rook_of_value = 7;
constexpr int rook_hf_value = 18;
constexpr int rook_battery_value = 7;

constexpr int pawn_on_center_value = 12;
constexpr int pawn_on_center4_value = 6;
constexpr int center_attack_value = 9;
//constexpr int knight_attack_center = 4;
//constexpr int bishop_attack_center = 4;

constexpr int isolated_pawn_value = -10;
constexpr int blocked_pawn_value = -10;
constexpr int double_pawn_value = -10;
constexpr int backward_pawn_value = -10;
constexpr int pawn_chain_value = 3;

constexpr int knight_outpost_value = 24;
constexpr int knight_outpost_reach_value = 10;
constexpr int knight_on_edge_value = -8;

constexpr int bishop_pair_value = 3;
constexpr int bishop_wrong_color_value = -1;

constexpr int kings_pawn_value = 24;
constexpr int castled_king_value = 30;
constexpr int king_ring_sliding_attacked_value = -11;
constexpr int castle_right_value = 10;
constexpr int king_open_file_value = -26;
constexpr int king_pawn_storm_value = -13;

constexpr int passed_pawn_value = 32;

constexpr int bishop_trapped_value = -16;
constexpr int bishop_trapped_by_opponent_value = -60;
constexpr int rook_trapped_value = -32;
constexpr int knight_cornered_value = -16;

constexpr int threat_value = 20;

constexpr int king_safety_treshold = queen_value + knight_value;

static bitboard pawn_chains(const position& pos)
{
	return pos.pawns() & pos.pawn_attacks();
}

static bitboard opponent_pawn_chains(const position& pos)
{
	return pos.opponent_pawns() & pos.opponent_pawn_attacks();
}

static bitboard isolated_pawns(const position& pos)
{
	bitboard pawns = pos.pawns();

	pawns &= east_of(pos.half_open_files(0) & ~bb_file_h);
	pawns &= west_of(pos.half_open_files(0) & ~bb_file_a);

	return pawns;
}

static bitboard opponent_isolated_pawns(const position& pos)
{
	bitboard pawns = pos.opponent_pawns();

	pawns &= east_of(pos.half_open_files(1) & ~bb_file_h);
	pawns &= west_of(pos.half_open_files(1) & ~bb_file_a);

	return pawns;
}

static bitboard blocked_pawns()
{
	return north_of(pos.pawns()) & pos.opponent_pieces();
}

static bitboard opponent_blocked_pawns(const position& pos)
{
	return south_of(pos.opponent_pawns()) & pos.pieces();
}

static bitboard double_pawns(const position& pos)
{
	return north_of(fill_north(pos.pawns())) & pos.pawns();
}

static bitboard opponent_double_pawns(const position& pos)
{
	return south_of(fill_south(pos.opponent_pawns())) & pos.opponent_pawns();
}

static bitboard backward_pawns(const position& pos)
{
	bitboard pawns = pos.pawns();

	// No friendly pawns an adjacent files, next to - or behind
	pawns &= ~south_of(pos.pawn_attack_reach(0));

	// How far can a pawn advance, without being attacked by another pawn?
	bitboard advance = fill_north(pawns);
	advance &= ~fill_north(advance & pos.opponent_pawn_attacks());
	advance &= south_of(pos.pawn_attacks());

	// If it can't reach a square next to a friendly pawn, it is backwards
	pawns &= ~fill_south(advance);

	return pawns;
}

static bitboard opponent_backward_pawns(const position& pos)
{
	bitboard pawns = pos.opponent_pawn();
	pawns &= ~north_of(pos.pawn_attack_reach(1));

	bitboard advance = fill_south(pawns);
	advance &= ~fill_south(advance & pos.pawn_attacks());
	advance &= north_of(pos.opponent_pawn_attacks());
	pawns &= ~fill_north(advance);

	return pawns;
}

static bitboard outposts(const position& pos)
{
	return pos.pawn_attacks() & ~pos.pawn_attack_reach(1)
	    & ~(bb_rank_8 | bb_file_a | bb_file_h);
}

static bitboard opponent_outposts(const position& pos)
{
	return pos.opponent_pawn_attacks() & ~pos.pawn_attack_reach(0)
	    & ~(bb_rank_1 | bb_file_a | bb_file_h);
}

static bitboard knight_outposts(const position& pos)
{
	return pos.knights() & outposts(pos);
}

static bitboard opponent_knight_outposts(const position& pos)
{
	return pos.opponent_knights() & opponent_outposts(pos);
}

static bitboard knight_reach_outposts(const position& pos)
{
	return attack[knight] & outposts() & ~map[0];
}

static bitboard opponent_knight_reach_outposts(const position& pos)
{
	return attack[opponent_knight] & opponent_outposts() & ~map[1];
}

static bitboard passed_pawns(const position& pos)
{
	bitboard pawns = map[pawn];
	pawns &= bb_rank_7 | bb_rank_6 | bb_rank_5 | bb_rank_4;
	pawns &= ~pawn_attack_reach[1];
	pawns &= ~fill_south(map[opponent_pawn]);

	return pawns;
}

static bitboard opponent_passed_pawns(const position& pos)
{
	bitboard pawns = map[opponent_pawn];
	pawns &= bb_rank_2 | bb_rank_3 | bb_rank_4 | bb_rank_5;
	pawns &= ~pawn_attack_reach[0];
	pawns &= ~fill_north(map[pawn]);

	return pawns;
}

static bitboard rooks_on_half_open_files(const position& pos)
{
	return map[rook] & half_open_files[0];
}

static bitboard opponent_rooks_on_half_open_files(const position& pos)
{
	return map[opponent_rook] & half_open_files[1];
}

static bitboard rooks_on_open_files(const position& pos)
{
	return map[rook] & half_open_files[0] & half_open_files[1];
}

static bitboard opponent_rooks_on_open_files(const position& pos)
{
	return map[opponent_rook] & half_open_files[0] & half_open_files[1];
}

static bitboard rook_batteries(const position& pos)
{
	return map[rook] & attack[rook]
	    & half_open_files[0]
	    & south_of(fill_south(map[rook]));
}

static bitboard opponent_rook_batteries(const position& pos)
{
	return map[opponent_rook] & attack[opponent_rook]
	    & half_open_files[1]
	    & north_of(fill_north(map[opponent_rook]));
}

static bitboard pawns_on_center(const position& pos)
{
	return map[pawn] & center_sq;
}

static bitboard opponent_pawns_on_center(const position& pos)
{
	return map[opponent_pawn] & center_sq;
}

static bitboard pawns_on_center4(const position& pos)
{
	return map[pawn] & attack[pawn] & center4_sq;
}

static bitboard opponent_pawns_on_center4(const position& pos)
{
	return map[opponent_pawn] & attack[opponent_pawn] & center4_sq;
}

static bitboard center4_attacks(const position& pos)
{
	return (attack[knight] | attack[bishop]) & center4_sq;
}

static bitboard opponent_center4_attacks(const position& pos)
{
	return (attack[opponent_knight] | attack[opponent_bishop]) & center4_sq;
}

static bool has_bishop_pair(const position& pos)
{
	return is_nonempty(map[bishop] & black_squares)
	    and is_nonempty(map[bishop] & white_squares);
}

static bool opponent_has_bishop_pair(const position& pos)
{
	return is_nonempty(map[opponent_bishop] & black_squares)
	    and is_nonempty(map[opponent_bishop] & white_squares);
}

static bitboard pawns_on_white(const position& pos)
{
	return white_squares & all_pawns();
}

static bitboard pawns_on_black(const position& pos)
{
	return black_squares & all_pawns();
}

static bitboard bishops_on_white(const position& pos)
{
	return white_squares & map[bishop];
}

static bitboard bishops_on_black(const position& pos)
{
	return black_squares & map[bishop];
}

static bitboard opponent_bishops_on_white(const position& pos)
{
	return white_squares & map[opponent_bishop];
}

static bitboard opponent_bishops_on_black(const position& pos)
{
	return black_squares & map[opponent_bishop];
}

static bitboard free_squares(const position& pos)
{
	return (~attack[1] | (attack[pawn] & ~attack[opponent_pawn])) & ~map[0];
}

static bitboard opponent_free_squares(const position& pos)
{
	return (~attack[0] | (attack[opponent_pawn] & ~attack[pawn])) & ~map[1];
}

static bool bishop_c1_is_trapped(const position& pos)
{
	return (map[pawn] & bb(b2, d2)) == bb(b2, d2);
}

static bool bishop_f1_is_trapped(const position& pos)
{
	return (map[pawn] & bb(e2, g2)) == bb(e2, g2);
}

static bool opponent_bishop_c8_is_trapped(const position& pos)
{
	return (map[opponent_pawn] & bb(b7, d7)) == bb(b7, d7);
}

static bool opponent_bishop_f8_is_trapped(const position& pos)
{
	return (map[opponent_pawn] & bb(e7, g7)) == bb(e7, g7);
}

static bool bishop_trapped_at_a7(const position& pos)
{
	return is_nonempty(map[bishop] & bb(a7))
	    and ((map[opponent_pawn] & bb(b6, c7)) == bb(b6, c7));
}

static bool bishop_trapped_at_h7(const position& pos)
{
	return is_nonempty(map[bishop] & bb(h7))
	    and ((map[opponent_pawn] & bb(g6, f7)) == bb(g6, f7));
}

static bool opponent_bishop_trapped_at_a2(const position& pos)
{
	return is_nonempty(map[opponent_bishop] & bb(a2))
	    and ((map[pawn] & bb(b3, c2)) == bb(b3, c2));
}

static bool opponent_bishop_trapped_at_h2(const position& pos)
{
	return is_nonempty(map[opponent_bishop] & bb(h2))
	    and ((map[pawn] & bb(g3, f2)) == bb(g3, f2));
}

static bool bishop_trappable_at_a7(const position& pos)
{
	return is_nonempty(map[bishop] & bb(a7))
	    and ((map[opponent_pawn] & bb(b7, c7)) == bb(b7, c7));
}

static bool bishop_trappable_at_h7(const position& pos)
{
	return is_nonempty(map[bishop] & bb(h7))
	    and ((map[opponent_pawn] & bb(g7, f7)) == bb(g7, f7));
}

static bool opponent_bishop_trappable_at_a2(const position& pos)
{
	return is_nonempty(map[opponent_bishop] & bb(a2))
	    and ((map[pawn] & bb(b2, c2)) == bb(b2, c2));
}

static bool opponent_bishop_trappable_at_h2(const position& pos)
{
	return is_nonempty(map[opponent_bishop] & bb(h2))
	    and ((map[pawn] & bb(g2, f2)) == bb(g2, f2));
}

static bool rook_a1_is_trapped(const position& pos)
{
	if (cr_queen_side)
		return false;

	bitboard r = map[rook] & bb(a1, b1, c1);
	bitboard trap = east_of(r) | east_of(east_of(r)) | bb(d1);
	bitboard blockers = map[king] | map[bishop] | map[knight];
	return is_nonempty(r)
	    and is_empty(r & half_open_files[0])
	    and is_nonempty(trap & blockers);
}

static bool rook_h1_is_trapped(const position& pos)
{
	if (cr_king_side)
		return false;

	bitboard r = map[rook] & bb(f1, g1, h1);
	bitboard trap = west_of(r) | west_of(west_of(r)) | bb(e1);
	bitboard blockers = map[king] | map[bishop] | map[knight];
	return is_nonempty(r)
	    and is_empty(r & half_open_files[0])
	    and is_nonempty(trap & blockers);
}

static bool opponent_rook_a8_is_trapped(const position& pos)
{
	if (cr_opponent_queen_side)
		return false;

	bitboard r = map[opponent_rook] & bb(a8, b8, c8);
	bitboard trap = east_of(r) | east_of(east_of(r)) | bb(d8);
	bitboard blockers =
	    map[opponent_king] | map[opponent_bishop] | map[opponent_knight];
	return is_nonempty(r)
	    and is_empty(r & half_open_files[1])
	    and is_nonempty(trap & blockers);
}

static bool opponent_rook_h8_is_trapped(const position& pos)
{
	if (cr_opponent_king_side)
		return false;

	bitboard r = map[opponent_rook] & bb(f8, g8, h8);
	bitboard trap = west_of(r) | west_of(west_of(r)) | bb(e8);
	bitboard blockers =
	    map[opponent_king] | map[opponent_bishop] | map[opponent_knight];
	return is_nonempty(r)
	    and is_empty(r & half_open_files[1])
	    and is_nonempty(trap & blockers);
}

static bool knight_cornered_a8(const position& pos)
{
	return is_nonempty(map[knight] & bb(a8))
	    and is_nonempty(attack[opponent_pawn] & bb(b6))
	    and is_nonempty(attack[1] & bb(c7));
}

static bool knight_cornered_h8(const position& pos)
{
	return is_nonempty(map[knight] & bb(h8))
	    and is_nonempty(attack[opponent_pawn] & bb(g6))
	    and is_nonempty(attack[1] & bb(f7));
}

static bool opponent_knight_cornered_a1(const position& pos)
{
	return is_nonempty(map[opponent_knight] & bb(a1))
	    and is_nonempty(attack[pawn] & bb(b3))
	    and is_nonempty(attack[0] & bb(c2));
}

static bool opponent_knight_cornered_h1(const position& pos)
{
	return is_nonempty(pos.opponent_knights() & bb(h1))
	    and is_nonempty(pos.attacks_of(pawn) & bb(g3))
	    and is_nonempty(pos.attacks_of(0) & bb(f2));
}


static int
eval_basic_mobility(const position *pos)
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
		uint64_t other_side = bb_rank_6 | bb_rank_7 | bb_rank_8;
		value += 6 * popcnt(pos->attack[pawn] & other_side);
		value += 3 * popcnt(pos->attack[bishop] & other_side);
		value += 3 * popcnt(pos->attack[knight] & other_side);
		value += 3 * popcnt(pos->attack[rook] & other_side);
	}

	if (opponent_non_pawn_material(pos) > rook_value * 3) {
		uint64_t other_side = bb_rank_3 | bb_rank_2 | bb_rank_1;
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

	free_sq &= center4_sq;
	value += popcnt(free_sq);
	value += 4 * popcnt(pos->attack[bishop] & free_sq);
	value += 4 * popcnt(pos->attack[knight] & free_sq);

	free_sq = opponent_free_squares(pos);

	value -= popcnt(free_sq);
	value -= popcnt(pos->attack[opponent_bishop] & free_sq);
	value -= popcnt(pos->attack[opponent_knight] & free_sq);
	value -= popcnt(pos->attack[opponent_rook] & free_sq);

	free_sq &= center4_sq;
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
	if (is_nonempty(king_map & bb(g1, h1))
	    && is_empty(rooks & bb(h1)))
		value += castled_king_value;
	else if (is_nonempty(king_map & bb(a1, c1, b1))
	    && is_empty(rooks & bb(a1)))
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
	    | north_of(west_of(king_map & ~bb_file_h))
	    | north_of(east_of(king_map & ~bb_file_a));

	// Penalty for not having any pawn on a file around the king
	value += king_open_file_value * popcnt(shield & half_open_files);

	// More Penalty for the king itself residing on an open file
	if (is_nonempty(king_map & half_open_files))
		value += king_open_file_value;

	// Penalty for opponent pawns advancing towards the king
	uint64_t storm = kogge_stone_north(shield) & opp_pawns;
	storm &= bb_rank_2 | bb_rank_3 | bb_rank_4 | bb_rank_5;
	value += king_pawn_storm_value * popcnt(storm);
	value += king_pawn_storm_value * popcnt(storm & ~bb_rank_5);
	value += king_pawn_storm_value * popcnt(storm & opp_sattacks & ~bb_rank_5);
	value += king_pawn_storm_value * popcnt(storm & north_of(shield));

	return value;
}

static int
king_safety_wrapper(const struct position *pos)
{
	if (is_empty(pos->map[king] & (bb_rank_1|bb_rank_2)))
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
	if (is_empty(pos->map[opponent_king] & (bb_rank_8|bb_rank_7)))
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
	return popcnt(kogge_stone_north(bitboard) & bb_rank_8);
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
	    * popcnt(pos->map[knight] & edges);
	value -= knight_on_edge_value
	    * popcnt(pos->map[opponent_knight] & edges);

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
	value += (passed_pawn_value / 2) * popcnt(pawns & bb_rank_6);
	value -= (passed_pawn_value / 2)* popcnt(opp_pawns & bb_rank_3);
	value += passed_pawn_value * popcnt(pawns & bb_rank_7);
	value -= passed_pawn_value * popcnt(opp_pawns & bb_rank_2);

	pawns &= pos->attack[pawn];
	opp_pawns &= pos->attack[opponent_pawn];

	value += (passed_pawn_value / 3) * popcnt(pawns & bb_rank_6);
	value -= (passed_pawn_value / 3) * popcnt(opp_pawns & bb_rank_3);
	value += (passed_pawn_value / 2) * popcnt(pawns & bb_rank_7);
	value -= (passed_pawn_value / 2) * popcnt(opp_pawns & bb_rank_2);

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

}
