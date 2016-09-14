
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "macros.h"
#include "bitboard.h"
#include "chess.h"
#include "position.h"

#include "move_gen_const.inc"

struct magical bitboard_magics[128];

static move*
add_moves_g(move *moves, int src_i, const struct position *pos,
		int piece, uint64_t dst_map)
{
	if (is_empty(dst_map))
		return moves;
	move m = create_move_g(src_i, 0, piece, 0);
	do {
		int to = bsf(dst_map);
		*moves++ = set_capturedp(set_mto(m, to), pos_piece_at(pos, to));
	} while (is_nonempty(dst_map = reset_lsb(dst_map)));
	return moves;
}

static move*
gen_king_moves(const struct position *pos, move *moves, uint64_t dst_mask)
{
	return add_moves_g(moves, pos->king_index, pos, king,
	    pos->attack[king] & dst_mask & ~pos->attack[1]);
}

static move*
gen_castle_queen_side(const struct position *pos, move *moves)
{
	if (pos->cr_queen_side
	    && is_empty((SQ_B1|SQ_C1|SQ_D1) & pos->occupied)
	    && is_empty((SQ_C1|SQ_D1) & pos->attack[1]))
		*moves++ = mcastle_queen_side;
	return moves;
}

static move*
gen_castle_king_side(const struct position *pos, move *moves)
{
	if (pos->cr_king_side
	    && is_empty((SQ_F1|SQ_G1) & pos->occupied)
	    && is_empty((SQ_F1|SQ_G1) & pos->attack[1]))
		*moves++ = mcastle_king_side;
	return moves;
}

static bool
can_en_passant_from(const struct position *pos, int delta)
{
	int pawni = pos->ep_index + delta;
	return (rank_5 == ind_rank(pawni))
	    && is_nonempty(bit64(pawni) & pos->map[pawn])
	    && is_empty(bit64(pawni) & pos->rpin_map);
}

static bool
can_en_passant_from_nopin(const struct position *pos, int delta)
{
	int pawni = pos->ep_index + delta;
	return (rank_5 == ind_rank(pawni))
	    && is_nonempty(bit64(pawni) & pos->map[pawn]);
}

static move*
gen_en_passant(const struct position *pos, move *moves, uint64_t dst_mask)
{
	if (!pos_has_ep_target(pos))
		return moves;

	uint64_t sq_dst = bit64(pos->ep_index);

	if (is_empty(dst_mask & (sq_dst | north_of(sq_dst))))
		return moves;
	if (is_empty(dst_mask & sq_dst) || is_nonempty(pos->bpin_map & sq_dst))
		return moves;

	if (can_en_passant_from(pos, EAST)) {
		uint64_t sq_pawn = east_of(sq_dst);
		uint64_t move_mask = north_of(sq_dst) | sq_pawn;
		uint64_t masked = pos->bpin_map & move_mask;
		if (is_empty(masked) || (masked == move_mask))
			*moves++ = create_move_t(pos->ep_index + EAST,
			    pos->ep_index + NORTH,
			    mt_en_passant, pawn, pawn);
	}
	if (can_en_passant_from(pos, WEST)) {
		uint64_t sq_pawn = west_of(sq_dst);
		uint64_t move_mask = north_of(sq_dst) | sq_pawn;
		uint64_t masked = pos->bpin_map & move_mask;
		if (is_empty(masked) || (masked == move_mask))
			*moves++ = create_move_t(pos->ep_index + WEST,
			    pos->ep_index + NORTH,
			    mt_en_passant, pawn, pawn);
	}
	return moves;
}

static move*
gen_en_passant_nopin(const struct position *pos, move *moves, uint64_t dst_mask)
{
	if (!pos_has_ep_target(pos))
		return moves;

	uint64_t sq_dst = bit64(pos->ep_index);

	if (is_empty(dst_mask & (sq_dst | north_of(sq_dst))))
		return moves;

	if (can_en_passant_from_nopin(pos, EAST))
		*moves++ = create_move_t(pos->ep_index + EAST,
		    pos->ep_index + NORTH,
		    mt_en_passant, pawn, pawn);
	if (can_en_passant_from_nopin(pos, WEST))
		*moves++ = create_move_t(pos->ep_index + WEST,
		    pos->ep_index + NORTH,
		    mt_en_passant, pawn, pawn);
	return moves;
}

static move*
add_promotions(move *moves, int from, int to, int captured)
{
	move m = create_move_t(from, to, mt_promotion, 0, captured);
	*moves++ = set_resultp(m, queen);
	*moves++ = set_resultp(m, knight);
	*moves++ = set_resultp(m, rook);
	*moves++ = set_resultp(m, bishop);
	return moves;
}

static bool
pawn_pinned(uint64_t pins, int from, int to)
{
	uint64_t move_map = bit64(from) | bit64(to);
	return is_nonempty(bit64(from) & pins)
	    && ((pins & move_map) != move_map);
}

static move*
gen_promotions(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t pawns = pos->map[pawn] & ~pos->rpin_map
	    & ~pos->bpin_map & RANK_7;
	uint64_t attacks = north_of(pawns) & ~pos->occupied & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		moves = add_promotions(moves, dst + SOUTH, dst, 0);
	}

	pawns = pos->map[pawn] & ~pos->rpin_map & RANK_7;
	attacks = north_of(west_of(pawns & ~FILE_A));
	attacks &= pos->map[1] & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + EAST;
		if (!pawn_pinned(pos->bpin_map, src, dst))
			moves = add_promotions(moves, src, dst,
			    pos_piece_at(pos, dst));
	}

	attacks = north_of(east_of(pawns & ~FILE_H));
	attacks &= pos->map[1] & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + WEST;
		if (!pawn_pinned(pos->bpin_map, src, dst))
			moves = add_promotions(moves, src, dst,
			    pos_piece_at(pos, dst));
	}

	return moves;
}

static move*
gen_promotions_nopin(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t pawns = pos->map[pawn] & RANK_7;
	if (is_empty(pawns))
		return moves;
	uint64_t attacks = north_of(pawns) & ~pos->occupied & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		moves = add_promotions(moves, dst + SOUTH, dst, 0);
	}

	attacks = north_of(west_of(pawns & ~FILE_A));
	attacks &= pos->map[1] & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + EAST;
		moves = add_promotions(moves, src, dst, pos_piece_at(pos, dst));
	}

	attacks = north_of(east_of(pawns & ~FILE_H));
	attacks &= pos->map[1] & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + WEST;
		moves = add_promotions(moves, src, dst, pos_piece_at(pos, dst));
	}

	return moves;
}

static move*
gen_pawn_pushes(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t pawns = pos->map[pawn] & ~pos->bpin_map & ~RANK_7;
	uint64_t pushes = north_of(pawns) & ~pos->occupied;

	for (uint64_t t = pushes & dst_mask; is_nonempty(t); t = reset_lsb(t)) {
		int dst = bsf(t);
		int src = dst + SOUTH;
		if (!pawn_pinned(pos->rpin_map, src, dst))
			*moves++ = create_move_g(src, dst, pawn, 0);
	}
	pushes = north_of(pawns) & ~pos->occupied;
	pushes = north_of(pushes) & RANK_4 & dst_mask & ~pos->occupied;
	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		int dst = bsf(pushes);
		int src = dst + SOUTH + SOUTH;
		if (!pawn_pinned(pos->rpin_map, src, dst))
			*moves++ = create_move_t(src, dst,
			    mt_pawn_double_push, pawn, 0);
	}
	return moves;
}

static move*
gen_pawn_pushes_nopin(const struct position *pos,
			move *moves, uint64_t dst_mask)
{
	uint64_t pushes = north_of(pos->map[pawn] & ~RANK_7)
	    & ~pos->occupied & dst_mask;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		int dst = bsf(pushes);
		int src = dst + SOUTH;
		*moves++ = create_move_g(src, dst, pawn, 0);
	}
	pushes = north_of(north_of(pos->map[pawn]) & ~pos->occupied)
	    & RANK_4 & dst_mask & ~pos->occupied;
	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		int dst = bsf(pushes);
		int src = dst + SOUTH + SOUTH;
		*moves++ = create_move_t(src, dst,
		    mt_pawn_double_push, pawn, 0);
	}
	return moves;
}

static move*
gen_pawn_captures(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t pawns = pos->map[pawn] & ~FILE_A & ~pos->rpin_map & ~RANK_7;
	uint64_t attacks = north_of(west_of(pawns)) & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + EAST;
		if (!pawn_pinned(pos->bpin_map, src, dst))
			*moves++ = create_move_g(src, dst, pawn,
			    pos_piece_at(pos, dst));
	}

	pawns = pos->map[pawn] & ~FILE_H & ~pos->rpin_map & ~RANK_7;
	attacks = north_of(east_of(pawns)) & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + WEST;
		if (!pawn_pinned(pos->bpin_map, src, dst))
			*moves++ = create_move_g(src, dst, pawn,
			    pos_piece_at(pos, dst));
	}

	return moves;
}

static move*
gen_pawn_captures_nopin(const struct position *pos,
			move *moves, uint64_t dst_mask)
{
	uint64_t pawns = pos->map[pawn] & ~FILE_A & ~RANK_7;
	uint64_t attacks = north_of(west_of(pawns)) & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + EAST;
		*moves++ = create_move_g(src, dst, pawn,
		    pos_piece_at(pos, dst));
	}

	pawns = pos->map[pawn] & ~FILE_H & ~RANK_7;
	attacks = north_of(east_of(pawns)) & dst_mask;

	for (; is_nonempty(attacks); attacks = reset_lsb(attacks)) {
		int dst = bsf(attacks);
		int src = dst + SOUTH + WEST;
		*moves++ = create_move_g(src, dst, pawn,
		    pos_piece_at(pos, dst));
	}

	return moves;
}

static move*
gen_knight_moves(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t knights = pos->map[knight] & ~pos->rpin_map & ~pos->bpin_map;
	for (; is_nonempty(knights); knights = reset_lsb(knights)) {
		int src = bsf(knights);
		moves = add_moves_g(moves, src, pos, knight,
		    knight_pattern(src) & dst_mask);
	}
	return moves;
}

static move*
gen_knight_moves_nopin(const struct position *pos,
			move *moves, uint64_t dst_mask)
{
	uint64_t knights = pos->map[knight];
	for (; is_nonempty(knights); knights = reset_lsb(knights)) {
		int src = bsf(knights);
		moves = add_moves_g(moves, src, pos, knight,
		    knight_pattern(src) & dst_mask);
	}
	return moves;
}

static move*
gen_bishop_moves(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t bishops = pos->map[bishop] & ~pos->rpin_map;

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		int src = bsf(bishops);
		uint64_t dst_map = dst_mask
		    & sliding_map(pos->occupied, bishop_magics + src);

		if (is_nonempty(bit64(src) & pos->bpin_map))
			dst_map &= pos->bpin_map;

		moves = add_moves_g(moves, src, pos, bishop, dst_map);
	}
	return moves;
}

static move*
gen_rook_moves(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t rooks = pos->map[rook] & ~pos->bpin_map;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		int src = bsf(rooks);
		uint64_t dst_map = dst_mask
		    & sliding_map(pos->occupied, rook_magics + src);

		if (is_nonempty(bit64(src) & pos->rpin_map))
			dst_map &= pos->rpin_map;

		moves = add_moves_g(moves, src, pos, rook, dst_map);
	}
	return moves;
}

static move*
gen_queen_moves(const struct position *pos, move *moves, uint64_t dst_mask)
{
	uint64_t queens = pos->map[queen];

	if (is_singular(queens)
	    && is_empty(queens & (pos->rpin_map | pos->bpin_map))) {
		uint64_t dst_map = pos->attack[queen] & dst_mask;
		moves = add_moves_g(moves, bsf(queens), pos, queen, dst_map);
	}
	else for (; is_nonempty(queens); queens = reset_lsb(queens)) {
		int src = bsf(queens);
		uint64_t dst_map = EMPTY;
		if (is_empty(lsb(queens) & pos->bpin_map))
			dst_map =
			    sliding_map(pos->occupied, rook_magics + src);
		if (is_empty(lsb(queens) & pos->rpin_map))
			dst_map =
			    sliding_map(pos->occupied, bishop_magics + src);

		dst_map &= dst_mask;
		if (is_nonempty(bit64(src) & pos->rpin_map))
			dst_map &= pos->rpin_map;
		else if (is_nonempty(bit64(src) & pos->bpin_map))
			dst_map &= pos->bpin_map;

		moves = add_moves_g(moves, src, pos, queen, dst_map);
	}
	return moves;
}

static move*
gen_bishop_moves_nopin(const struct position *pos,
			move *moves, uint64_t dst_mask)
{
	uint64_t bishops = pos->map[bishop];

	if (is_singular(bishops)) {
		moves = add_moves_g(moves, bsf(bishops), pos, bishop,
		    pos->attack[bishop] & dst_mask);
	}
	else for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		int src = bsf(bishops);
		uint64_t dst_map = dst_mask
		    & sliding_map(pos->occupied, bishop_magics + src);

		moves = add_moves_g(moves, src, pos, bishop, dst_map);
	}
	return moves;
}

static move*
gen_rook_moves_nopin(const struct position *pos,
			move *moves, uint64_t dst_mask)
{
	uint64_t rooks = pos->map[rook];

	if (is_singular(rooks)) {
		moves = add_moves_g(moves, bsf(rooks), pos, rook,
		    pos->attack[rook] & dst_mask);
	}
	else for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		int src = bsf(rooks);
		uint64_t dst_map = dst_mask
		    & sliding_map(pos->occupied, rook_magics + src);

		moves = add_moves_g(moves, src, pos, rook, dst_map);
	}
	return moves;
}

static move*
gen_queen_moves_nopin(const struct position *pos,
			move *moves, uint64_t dst_mask)
{
	uint64_t queens = pos->map[queen];

	if (is_singular(queens)) {
		moves = add_moves_g(moves, bsf(queens), pos, queen,
		    pos->attack[queen] & dst_mask);
	}
	else for (; is_nonempty(queens); queens = reset_lsb(queens)) {
		int src = bsf(queens);
		uint64_t dst_map = dst_mask &
		    (sliding_map(pos->occupied, rook_magics + src)
		    | sliding_map(pos->occupied, bishop_magics + src));
		moves = add_moves_g(moves, src, pos, queen, dst_map);
	}
	return moves;
}

static move*
gen_moves_general(const struct position *pos,
			move moves[static MOVE_ARRAY_LENGTH],
			uint64_t dest_mask)
{
	moves = gen_knight_moves(pos, moves, dest_mask);
	moves = gen_rook_moves(pos, moves, dest_mask);
	moves = gen_bishop_moves(pos, moves, dest_mask);

	moves = gen_queen_moves(pos, moves, dest_mask);

	moves = gen_pawn_captures(pos, moves, dest_mask & pos->map[1]);
	moves = gen_promotions(pos, moves, dest_mask);
	moves = gen_en_passant(pos, moves, dest_mask);
	return moves;
}

static move*
gen_moves_general_nopin(const struct position *pos,
			move moves[static MOVE_ARRAY_LENGTH],
			uint64_t dest_mask)
{
	moves = gen_knight_moves_nopin(pos, moves, dest_mask);
	moves = gen_rook_moves_nopin(pos, moves, dest_mask);
	moves = gen_bishop_moves_nopin(pos, moves, dest_mask);
	moves = gen_queen_moves_nopin(pos, moves, dest_mask);
	moves = gen_pawn_captures_nopin(pos, moves, dest_mask & pos->map[1]);
	moves = gen_promotions_nopin(pos, moves, dest_mask);
	moves = gen_pawn_pushes_nopin(pos, moves, dest_mask);
	moves = gen_en_passant_nopin(pos, moves, dest_mask);
	return moves;
}

unsigned
gen_moves(const struct position *pos, move moves[static MOVE_ARRAY_LENGTH])
{
	move *first = moves;
	if (popcnt(pos_king_attackers(pos)) <= 1) {
		uint64_t dest_mask;

		if (is_in_check(pos)) {
			dest_mask = pos->king_attack_map;
		}
		else {
			dest_mask = ~pos->map[0];
			moves = gen_castle_king_side(pos, moves);
			moves = gen_castle_queen_side(pos, moves);
		}
		if (is_empty(pos->bpin_map) && is_empty(pos->rpin_map)) {
			moves = gen_moves_general_nopin(pos, moves, dest_mask);
		}
		else {
			moves = gen_moves_general(pos, moves, dest_mask);
			moves = gen_pawn_pushes(pos, moves, dest_mask);
		}
	}
	moves = gen_king_moves(pos, moves, ~pos->map[0]);
	*moves = 0;

	return moves - first;
}

unsigned
gen_captures(const struct position *pos,
		move moves[static MOVE_ARRAY_LENGTH])
{
	move *first = moves;
	uint64_t dest_mask = pos->map[1];
	if (is_nonempty(pos->attack[0] & dest_mask)) {
		moves = gen_moves_general(pos, moves, dest_mask);
		moves = gen_king_moves(pos, moves, dest_mask);
	}
	else {
		moves = gen_en_passant(pos, moves, dest_mask);
	}
	*moves = 0;

	return moves - first;
}

static void
init_sliding_move_magics(struct magical *dst, const uint64_t *raw_info,
#ifdef SLIDING_BYTE_LOOKUP
	const uint8_t *byte_lookup_table,
#endif
	const uint64_t *table)
{
	dst->mask = raw_info[0];
	dst->multiplier = raw_info[1];
	dst->shift = (int)(raw_info[2] & 0xff);
#ifdef SLIDING_BYTE_LOOKUP
	dst->attack_table = table + raw_info[3];
	dst->attack_index_table = byte_lookup_table + (raw_info[2]>>8);
#else
	dst->attack_table = table + (raw_info[2]>>8);
#endif
}

void
init_move_gen(void)
{
#ifdef SLIDING_BYTE_LOOKUP
	static const int magic_block = 4;
#else
	static const int magic_block = 3;
#endif
	for (int i = 0; i < 64; ++i) {
		init_sliding_move_magics(rook_magics + i,
		    rook_magics_raw + magic_block * i,
#ifdef SLIDING_BYTE_LOOKUP
		    rook_attack_index8,
#endif
		    rook_magic_attacks);
		init_sliding_move_magics(bishop_magics + i,
		    bishop_magics_raw + magic_block * i,
#ifdef SLIDING_BYTE_LOOKUP
		    bishop_attack_index8,
#endif
		    bishop_magic_attacks);
	}
}
