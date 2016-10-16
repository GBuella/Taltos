
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_EVAL_TERMS_H
#define TALTOS_EVAL_TERMS_H

#include "position.h"
#include "constants.h"
#include "eval.h"

static inline uint64_t
pawn_chains(const struct position *pos)
{
	return pos->map[pawn] & pos->attack[pawn];
}

static inline uint64_t
opponent_pawn_chains(const struct position *pos)
{
	return pos->map[opponent_pawn] & pos->attack[opponent_pawn];
}

static inline uint64_t
isolated_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[pawn];

	pawns &= east_of(pos->half_open_files[0] & ~FILE_H);
	pawns &= west_of(pos->half_open_files[0] & ~FILE_A);

	return pawns;
}

static inline uint64_t
opponent_isolated_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[opponent_pawn];

	pawns &= east_of(pos->half_open_files[1] & ~FILE_H);
	pawns &= west_of(pos->half_open_files[1] & ~FILE_A);

	return pawns;
}

static inline uint64_t
blocked_pawns(const struct position *pos)
{
	return north_of(pos->map[pawn]) & pos->map[1];
}

static inline uint64_t
opponent_blocked_pawns(const struct position *pos)
{
	return north_of(pos->map[opponent_pawn]) & pos->map[0];
}

static inline uint64_t
double_pawns(const struct position *pos)
{
	return north_of(kogge_stone_north(pos->map[pawn])) & pos->map[pawn];
}

static inline uint64_t
opponent_double_pawns(const struct position *pos)
{
	return north_of(kogge_stone_north(pos->map[opponent_pawn]))
	    & pos->map[opponent_pawn];
}

static inline uint64_t
backward_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[pawn];

	// No friendly pawns an adjacent files, next to - or behind
	pawns &= ~south_of(pos->pawn_attack_reach[0]);

	// How far can a pawn advance, without being attacked by another pawn?
	uint64_t advance = kogge_stone_north(pawns);
	advance &= ~kogge_stone_north(advance & pos->attack[opponent_pawn]);
	advance &= south_of(pos->attack[pawn]);

	// If it can't reach a square next to a friendly pawn, it is backwards
	pawns &= ~kogge_stone_south(advance);

	return pawns;
}

static inline uint64_t
opponent_backward_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[opponent_pawn];
	pawns &= ~north_of(pos->pawn_attack_reach[1]);

	uint64_t advance = kogge_stone_south(pawns);
	advance &= ~kogge_stone_south(advance & pos->attack[pawn]);
	advance &= north_of(pos->attack[opponent_pawn]);
	pawns &= ~kogge_stone_north(advance);

	return pawns;
}

static inline uint64_t
outposts(const struct position *pos)
{
	return pos->attack[pawn]
	    & ~pos->pawn_attack_reach[1]
	    & ~(RANK_8 | FILE_A | FILE_H);
}

static inline uint64_t
opponent_outposts(const struct position *pos)
{
	return pos->attack[opponent_pawn]
	    & ~pos->pawn_attack_reach[0]
	    & ~(RANK_1 | FILE_A | FILE_H);
}

static inline uint64_t
knight_outposts(const struct position *pos)
{
	return pos->map[knight] & outposts(pos);
}

static inline uint64_t
opponent_knight_outposts(const struct position *pos)
{
	return pos->map[opponent_knight] & opponent_outposts(pos);
}

static inline uint64_t
knight_reach_outposts(const struct position *pos)
{
	return pos->attack[knight] & outposts(pos)
	    & ~pos->map[0];
}

static inline uint64_t
opponent_knight_reach_outposts(const struct position *pos)
{
	return pos->attack[opponent_knight] & opponent_outposts(pos)
	    & ~pos->map[1];
}

static inline uint64_t
passed_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[pawn];
	pawns &= (RANK_7 | RANK_6 | RANK_5 | RANK_4);
	pawns &= ~pos->pawn_attack_reach[1];
	pawns &= ~kogge_stone_south(pos->map[opponent_pawn]);

	return pawns;
}

static inline uint64_t
opponent_passed_pawns(const struct position *pos)
{
	uint64_t pawns = pos->map[opponent_pawn];
	pawns &= (RANK_2 | RANK_3 | RANK_4 | RANK_5);
	pawns &= ~pos->pawn_attack_reach[0];
	pawns &= ~kogge_stone_north(pos->map[pawn]);

	return pawns;
}

static inline uint64_t
rooks_on_half_open_files(const struct position *pos)
{
	return pos->map[rook] & pos->half_open_files[0];
}

static inline uint64_t
opponent_rooks_on_half_open_files(const struct position *pos)
{
	return pos->map[opponent_rook] & pos->half_open_files[1];
}

static inline uint64_t
rooks_on_open_files(const struct position *pos)
{
	return pos->map[rook] &
	    pos->half_open_files[0] & pos->half_open_files[1];
}

static inline uint64_t
opponent_rooks_on_open_files(const struct position *pos)
{
	return pos->map[opponent_rook] &
	    pos->half_open_files[0] & pos->half_open_files[1];
}

static inline uint64_t
rook_batteries(const struct position *pos)
{
	return pos->map[rook] & pos->attack[rook]
	    & pos->half_open_files[0]
	    & south_of(kogge_stone_south(pos->map[rook]));
}

static inline uint64_t
opponent_rook_batteries(const struct position *pos)
{
	return pos->map[opponent_rook] & pos->attack[opponent_rook]
	    & pos->half_open_files[1]
	    & south_of(kogge_stone_south(pos->map[opponent_rook]));
}

static inline uint64_t
pawns_on_center(const struct position *pos)
{
	return pos->map[pawn] & pos->attack[pawn] & CENTER_SQ;
}

static inline uint64_t
opponent_pawns_on_center(const struct position *pos)
{
	return pos->map[opponent_pawn] & pos->attack[opponent_pawn] & CENTER_SQ;
}

static inline uint64_t
pawns_on_center4(const struct position *pos)
{
	return pos->map[pawn] & pos->attack[pawn] & CENTER4_SQ;
}

static inline uint64_t
opponent_pawns_on_center4(const struct position *pos)
{
	return pos->map[opponent_pawn] & pos->attack[opponent_pawn]
	    & CENTER4_SQ;
}

static inline uint64_t
center4_attacks(const struct position *pos)
{
	return (pos->attack[knight] | pos->attack[bishop]) & CENTER4_SQ;
}

static inline uint64_t
opponent_center4_attacks(const struct position *pos)
{
	return (pos->attack[opponent_knight] | pos->attack[opponent_bishop])
	    & CENTER4_SQ;
}

static inline bool
has_bishop_pair(const struct position *pos)
{
	return is_nonempty(pos->map[bishop] & BLACK_SQUARES)
	    && is_nonempty(pos->map[bishop] & WHITE_SQUARES);
}

static inline bool
opponent_has_bishop_pair(const struct position *pos)
{
	return is_nonempty(pos->map[opponent_bishop] & BLACK_SQUARES)
	    && is_nonempty(pos->map[opponent_bishop] & WHITE_SQUARES);
}

static inline uint64_t
all_pawns(const struct position *pos)
{
	return pos->map[pawn] | pos->map[opponent_pawn];
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
bishops_on_white(const struct position *pos)
{
	return WHITE_SQUARES & pos->map[bishop];
}

static inline uint64_t
bishops_on_black(const struct position *pos)
{
	return BLACK_SQUARES & pos->map[bishop];
}

static inline uint64_t
opponent_bishops_on_white(const struct position *pos)
{
	return WHITE_SQUARES & pos->map[opponent_bishop];
}

static inline uint64_t
opponent_bishops_on_black(const struct position *pos)
{
	return BLACK_SQUARES & pos->map[opponent_bishop];
}

static inline uint64_t
free_squares(const struct position *pos)
{
	return ~(pos->map[0] | pos->attack[1]);
}

static inline uint64_t
opponent_free_squares(const struct position *pos)
{
	return ~(pos->map[1] | pos->attack[0]);
}

static inline int
non_pawn_material(const struct position *pos)
{
	return pos->material_value -
	    pawn_value * popcnt(pos->map[pawn]);
}

static inline int
opponent_non_pawn_material(const struct position *pos)
{
	return pos->opponent_material_value -
	    pawn_value * popcnt(pos->map[opponent_pawn]);
}

static inline bool
bishop_c1_is_trapped(const struct position *pos)
{
	return (pos->map[pawn] & (SQ_B1 | SQ_D1)) == (SQ_B1 | SQ_D1);
}

static inline bool
bishop_f1_is_trapped(const struct position *pos)
{
	return (pos->map[pawn] & (SQ_E1 | SQ_G1)) == (SQ_E1 | SQ_G1);
}

static inline bool
bishop_c8_is_trapped(const struct position *pos)
{
	return (pos->map[opponent_pawn] & (SQ_B8 | SQ_D8)) == (SQ_B8 | SQ_D8);
}

static inline bool
bishop_f8_is_trapped(const struct position *pos)
{
	return (pos->map[opponent_pawn] & (SQ_E8 | SQ_G8)) == (SQ_E8 | SQ_G8);
}

static inline bool
rook_a1_is_trapped(const struct position *pos)
{
	if (pos->cr_queen_side || pos->cr_king_side)
		return false;

	uint64_t r = pos->map[rook] & (SQ_A1 | SQ_B1 | SQ_C1);
	uint64_t trap = east_of(r) | east_of(east_of(r)) | SQ_D1;
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[0])
	    && is_nonempty(trap & pos->map[king]);
}

static inline bool
rook_h1_is_trapped(const struct position *pos)
{
	if (pos->cr_king_side || pos->cr_queen_side)
		return false;

	uint64_t r = pos->map[rook] & (SQ_F1 | SQ_G1 | SQ_H1);
	uint64_t trap = west_of(r) | west_of(west_of(r)) | SQ_E1;
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[0])
	    && is_nonempty(trap & pos->map[king]);
}

static inline bool
rook_a8_is_trapped(const struct position *pos)
{
	if (pos->cr_opponent_king_side || pos->cr_opponent_queen_side)
		return false;

	uint64_t r = pos->map[opponent_rook] & (SQ_A8 | SQ_B8 | SQ_C8);
	uint64_t trap = east_of(r) | east_of(east_of(r)) | SQ_D8;
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[1])
	    && is_nonempty(trap & pos->map[opponent_king]);
}

static inline bool
rook_h8_is_trapped(const struct position *pos)
{
	if (pos->cr_opponent_king_side || pos->cr_opponent_queen_side)
		return false;

	uint64_t r = pos->map[opponent_rook] & (SQ_F8 | SQ_G8 | SQ_H8);
	uint64_t trap = west_of(r) | west_of(west_of(r)) | SQ_E8;
	return is_nonempty(r)
	    && is_empty(r & pos->half_open_files[1])
	    && is_nonempty(trap & pos->map[opponent_king]);
}

#endif
