
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

struct move_gen {
	const struct position *pos;
	move *moves;
	uint64_t dst_mask;
	int opp_ki;
	uint64_t bishop_movers;
	uint64_t rook_movers;
	uint64_t opp_king_pawn_reach;
	uint64_t opp_king_knight_reach;
	uint64_t opp_king_bishop_reach;
	uint64_t opp_king_rook_reach;
};

static bool
revealed_bcheck(struct move_gen *mg, uint64_t remove_mask, uint64_t add_mask)
{
	if (is_empty(remove_mask & mg->opp_king_bishop_reach))
		return false;

	uint64_t occ = (mg->pos->occupied | add_mask) & ~remove_mask;
	uint64_t new_reach = sliding_map(occ, bishop_magics + mg->opp_ki);

	return is_nonempty(new_reach & mg->bishop_movers);
}

static bool
revealed_rcheck(struct move_gen *mg, uint64_t remove_mask, uint64_t add_mask)
{
	if (is_empty(remove_mask & mg->opp_king_rook_reach))
		return false;

	uint64_t occ = (mg->pos->occupied | add_mask) & ~remove_mask;
	uint64_t new_reach = sliding_map(occ, rook_magics + mg->opp_ki);

	return is_nonempty(new_reach & mg->rook_movers);
}

static void
gen_king_moves(struct move_gen *mg, uint64_t dst_mask)
{
	uint64_t from64 = mg->pos->map[king];
	uint64_t dsts = mg->pos->attack[king];
	dsts &= dst_mask & ~mg->pos->attack[1];

	for (; is_nonempty(dsts); dsts = reset_lsb(dsts)) {
		uint64_t to64 = lsb(dsts);
		bool gives_check = revealed_rcheck(mg, from64, to64)
		    || revealed_bcheck(mg, from64, to64);

		*mg->moves++ = create_move_g(mg->pos->king_index, bsf(to64),
		    king, mg->pos->board[bsf(to64)], gives_check);
	}
}

static void
gen_castle_queen_side(struct move_gen *mg)
{
	if (!(mg->pos->cr_queen_side
	    && is_empty((SQ_B1|SQ_C1|SQ_D1) & mg->pos->occupied)
	    && is_empty((SQ_C1|SQ_D1) & mg->pos->attack[1])))
		return;

	if (is_nonempty(mg->opp_king_rook_reach & SQ_D1)) {
		*mg->moves++ = mcastle_queen_side_check;
		return;
	}

	if (is_nonempty(mg->pos->map[opponent_king] & RANK_1)) {
		if (is_nonempty(mg->opp_king_rook_reach & SQ_E1)) {
			*mg->moves++ = mcastle_queen_side_check;
			return;
		}
	}

	*mg->moves++ = mcastle_queen_side;
}

static void
gen_castle_king_side(struct move_gen *mg)
{
	if (!(mg->pos->cr_king_side
	    && is_empty((SQ_F1|SQ_G1) & mg->pos->occupied)
	    && is_empty((SQ_F1|SQ_G1) & mg->pos->attack[1])))
		return;

	if (is_nonempty(mg->opp_king_rook_reach & SQ_F1)) {
		*mg->moves++ = mcastle_king_side_check;
		return;
	}

	if (is_nonempty(mg->pos->map[opponent_king] & RANK_1)) {
		if (is_nonempty(mg->opp_king_rook_reach & SQ_E1)) {
			*mg->moves++ = mcastle_king_side_check;
			return;
		}
	}

	*mg->moves++ = mcastle_king_side;
}

static void
gen_en_passant_dir(struct move_gen *mg, uint64_t from64, uint64_t to64,
			uint64_t victim, bool gives_check)
{
	if (!gives_check) {
		gives_check = is_nonempty(mg->opp_king_pawn_reach & to64)
		    || revealed_rcheck(mg, from64 | victim, to64)
		    || revealed_rcheck(mg, from64 | victim, to64);
	}

	*mg->moves++ = create_move_t(bsf(from64), bsf(to64),
		    mt_en_passant, pawn, pawn, gives_check);
}

static void
gen_en_passant(struct move_gen *mg)
{
	if (!pos_has_ep_target(mg->pos))
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
	if (is_nonempty(mg->pos->bpin_map & victim))
		return;

	bool check = is_nonempty(to64 & mg->opp_king_pawn_reach);
	check |= revealed_bcheck(mg, victim, 0);

	uint64_t attackers = pawn_reach_south(to64) & mg->pos->map[pawn];

	// Can't capture by pawn at all, when pinned on file or rank
	attackers &= ~mg->pos->rpin_map;

	/*
	 * The capture can happen along a diagonal pin ( when both
	 * from and to squares are on the the diagonal ). But if
	 * the from square is is along the pin, while the destination
	 * square is not, a check would be revealed. So filter out
	 * all diagonally pinned attackers, unless the destination
	 * is also on part of the diagonal.
	 */
	if (is_empty(mg->pos->bpin_map & to64))
		attackers &= ~mg->pos->bpin_map;

	for (; is_nonempty(attackers); attackers = reset_lsb(attackers))
		gen_en_passant_dir(mg, lsb(attackers), to64, victim, check);
}

static void
gen_en_passant_nopin(struct move_gen *mg)
{
	if (!pos_has_ep_target(mg->pos))
		return;

	uint64_t victim = bit64(mg->pos->ep_index);
	uint64_t to64 = north_of(victim);

	if (is_empty(mg->dst_mask & (to64 | victim)))
		return;

	bool check = is_nonempty(to64 & mg->opp_king_pawn_reach);
	check |= revealed_bcheck(mg, victim, 0);

	uint64_t attackers = pawn_reach_south(to64) & mg->pos->map[pawn];

	for (; is_nonempty(attackers); attackers = reset_lsb(attackers))
		gen_en_passant_dir(mg, lsb(attackers), to64, victim, check);
}

static void
gen_promotion_push(struct move_gen *mg, uint64_t from64)
{
	/*
	 * Pawn push promotions. The pawn can't be pinned at all - if it would
	 * be pinned by a rook in front of it, it wouldn't be able to move
	 * anyways. These pushes can give check in a way other moves can't:
	 * 8/P7/8/k7/8/7K/8/8 w - - 4 3 <-- white moves and gives check
	 * The A8 square is not in the mg->opp_king_rook_reach bitboard,
	 * but promoting to a queen or rook there by a push gives check anyways.
	 */
	uint64_t to64 = north_of(from64);
	int from = bsf(from64);
	int to = from + NORTH;

	bool revealed_check = revealed_bcheck(mg, from64, to64);
	revealed_check |= revealed_rcheck(mg, from64, to64);
	bool rcheck = is_nonempty(to64 & mg->opp_king_rook_reach);
	bool bcheck = is_nonempty(to64 & mg->opp_king_bishop_reach);
	rcheck |= revealed_check;
	bcheck |= revealed_check;
	uint64_t kncheck = is_nonempty(to64 & mg->opp_king_knight_reach);
	kncheck |= revealed_check;

	if (is_nonempty(mg->opp_king_rook_reach & from64)) {
		uint64_t behind = south_of(from64);
		rcheck |=
		    is_nonempty(mg->pos->map[opponent_king] & behind);
		rcheck |=
		    is_nonempty(mg->opp_king_rook_reach & behind);
	}

	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    queen, 0, rcheck | bcheck);
	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    rook, 0, rcheck);
	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    bishop, 0, bcheck);
	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    knight, 0, kncheck);
}

static void
gen_promotion_capture(struct move_gen *mg, uint64_t from64, uint64_t to64,
			uint64_t behind)
{
	/*
	 * Capturing pawn promotions.
	 * The pawn can be diagonally pinned, as long as the pinning
	 * piece is the one captured.
	 * These moves can also give check is a special way, much
	 * the pushes above.
	 */
	int from = bsf(from64);
	int to = bsf(to64);
	int captured = mg->pos->board[to];

	if (is_nonempty(mg->pos->bpin_map & from64)) {
		if (is_empty(mg->pos->bpin_map & to64))
			return;
	}

	bool revealed_check = revealed_bcheck(mg, from64, to64);
	revealed_check |= revealed_rcheck(mg, from64, to64);
	bool rcheck = is_nonempty(to64 & mg->opp_king_rook_reach);
	bool bcheck = is_nonempty(to64 & mg->opp_king_bishop_reach);
	rcheck |= revealed_check;
	bcheck |= revealed_check;
	uint64_t kncheck = is_nonempty(to64 & mg->opp_king_knight_reach);
	kncheck |= revealed_check;

	if (is_nonempty(mg->opp_king_bishop_reach & from64)) {
		bcheck |=
		    is_nonempty(mg->pos->map[opponent_king] & behind);
		bcheck |=
		    is_nonempty(mg->opp_king_bishop_reach & behind);
	}

	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    queen, captured, rcheck | bcheck);
	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    rook, captured, rcheck);
	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    bishop, captured, bcheck);
	*mg->moves++ = create_move_t(from, to, mt_promotion,
	    knight, captured, kncheck);
}

static void
gen_promotions(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn] & RANK_7;

	if (is_empty(pawns))
		return;

	pawns &= ~mg->pos->rpin_map & ~mg->pos->bpin_map;
	pawns &= south_of(mg->dst_mask & ~mg->pos->occupied);

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns))
		gen_promotion_push(mg, lsb(pawns));

	pawns = mg->pos->map[pawn] & ~mg->pos->rpin_map & RANK_7;
	pawns &= south_of(east_of(mg->pos->map[1] & mg->dst_mask & ~FILE_H));

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns)) {
		uint64_t pawn = lsb(pawns);
		uint64_t to64 = north_of(west_of(pawn));

		if (is_nonempty(mg->pos->bpin_map & pawn)) {
			if (is_empty(mg->pos->bpin_map & to64))
				continue;
		}

		gen_promotion_capture(mg, pawn, to64,
		    south_of(east_of(pawn)) & ~FILE_A);
	}

	pawns = mg->pos->map[pawn] & ~mg->pos->rpin_map & RANK_7;
	pawns &= south_of(west_of(mg->pos->map[1] & mg->dst_mask & ~FILE_A));

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns)) {
		uint64_t pawn = lsb(pawns);
		uint64_t to64 = north_of(east_of(pawn));

		if (is_nonempty(mg->pos->bpin_map & pawn)) {
			if (is_empty(mg->pos->bpin_map & to64))
				continue;
		}

		gen_promotion_capture(mg, pawn, to64,
		    south_of(west_of(pawn)) & ~FILE_H);
	}
}

static void
gen_promotions_nopin(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn] & RANK_7;

	if (is_empty(pawns))
		return;

	pawns &= south_of(mg->dst_mask & ~mg->pos->occupied);

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns))
		gen_promotion_push(mg, lsb(pawns));

	pawns = mg->pos->map[pawn] & RANK_7;
	pawns &= south_of(east_of(mg->pos->map[1] & mg->dst_mask & ~FILE_H));

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns)) {
		uint64_t pawn = lsb(pawns);
		uint64_t to64 = north_of(west_of(pawn));

		gen_promotion_capture(mg, pawn, to64,
		    south_of(east_of(pawn)) & ~FILE_A);
	}

	pawns = mg->pos->map[pawn] & RANK_7;
	pawns &= south_of(west_of(mg->pos->map[1] & mg->dst_mask & ~FILE_A));

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns)) {
		uint64_t pawn = lsb(pawns);
		uint64_t to64 = north_of(east_of(pawn));

		gen_promotion_capture(mg, pawn, to64,
		    south_of(west_of(pawn)) & ~FILE_H);
	}
}

static void
gen_pawn_push(struct move_gen *mg, enum move_type t,
		uint64_t from64, uint64_t to64)
{
	bool check = is_nonempty(mg->opp_king_pawn_reach & to64)
	    || revealed_bcheck(mg, from64, to64)
	    || revealed_rcheck(mg, from64, to64);

	*mg->moves++ = create_move_t(bsf(from64), bsf(to64), t, pawn, 0, check);
}

static void
gen_pawn_capture(struct move_gen *mg, uint64_t from64, uint64_t to64)
{
	bool check = is_nonempty(mg->opp_king_pawn_reach & to64)
	    || revealed_bcheck(mg, from64, to64)
	    || revealed_rcheck(mg, from64, to64);

	*mg->moves++ = create_move_g(bsf(from64), bsf(to64), pawn,
	    mg->pos->board[bsf(to64)], check);
}

static void
gen_pawn_pushes(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn] & ~mg->pos->bpin_map & ~RANK_7;
	uint64_t pushes = north_of(pawns) & ~mg->pos->occupied & mg->dst_mask;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		uint64_t to64 = lsb(pushes);
		uint64_t from64 = south_of(to64);

		if (is_nonempty(mg->pos->rpin_map & from64) &&
		    is_empty(mg->pos->rpin_map & to64))
			continue;

		gen_pawn_push(mg, mt_general, from64, to64);
	}

	pawns &= RANK_2;
	pushes = north_of(pawns) & ~mg->pos->occupied;
	pushes = north_of(pushes) & mg->dst_mask & ~mg->pos->occupied;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		uint64_t to64 = lsb(pushes);
		uint64_t from64 = south_of(south_of(to64));

		if (is_nonempty(mg->pos->rpin_map & from64) &&
		    is_empty(mg->pos->rpin_map & to64))
			continue;

		gen_pawn_push(mg, mt_pawn_double_push, from64, to64);
	}
}

static void
gen_pawn_pushes_nopin(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn] & ~RANK_7;
	uint64_t pushes = north_of(pawns) & ~mg->pos->occupied & mg->dst_mask;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		uint64_t to64 = lsb(pushes);
		gen_pawn_push(mg, mt_general, south_of(to64), to64);
	}

	pawns &= RANK_2;
	pushes = north_of(pawns) & ~mg->pos->occupied;
	pushes = north_of(pushes) & mg->dst_mask & ~mg->pos->occupied;

	for (; is_nonempty(pushes); pushes = reset_lsb(pushes)) {
		uint64_t to64 = lsb(pushes);
		gen_pawn_push(mg, mt_pawn_double_push,
		    south_of(south_of(to64)), to64);
	}
}

static void
gen_pawn_captures(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn] & ~mg->pos->rpin_map & ~RANK_7;
	uint64_t victims = mg->pos->map[1] & mg->dst_mask;
	victims &= north_of(west_of(pawns & ~FILE_A));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		uint64_t to64 = lsb(victims);
		uint64_t from64 = south_of(east_of(to64));

		if (is_nonempty(from64 & mg->pos->bpin_map))
			if (is_empty(to64 & mg->pos->bpin_map))
				continue;

		gen_pawn_capture(mg, from64, to64);
	}

	victims = mg->pos->map[1] & mg->dst_mask;
	victims &= north_of(east_of(pawns & ~FILE_H));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		uint64_t to64 = lsb(victims);
		uint64_t from64 = south_of(west_of(to64));

		if (is_nonempty(from64 & mg->pos->bpin_map))
			if (is_empty(to64 & mg->pos->bpin_map))
				continue;

		gen_pawn_capture(mg, from64, to64);
	}
}

static void
gen_pawn_captures_nopin(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn] & ~RANK_7;
	uint64_t victims = mg->pos->map[1] & mg->dst_mask;
	victims &= north_of(west_of(pawns & ~FILE_A));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		uint64_t to64 = lsb(victims);
		uint64_t from64 = south_of(east_of(to64));

		gen_pawn_capture(mg, from64, to64);
	}

	victims = mg->pos->map[1] & mg->dst_mask;
	victims &= north_of(east_of(pawns & ~FILE_H));

	for (; is_nonempty(victims); victims = reset_lsb(victims)) {
		uint64_t to64 = lsb(victims);
		uint64_t from64 = south_of(west_of(to64));

		gen_pawn_capture(mg, from64, to64);
	}
}

static void
gen_knight_moves(struct move_gen *mg)
{
	uint64_t knights = mg->pos->map[knight];
	knights &= ~mg->pos->rpin_map & ~mg->pos->bpin_map;

	for (; is_nonempty(knights); knights = reset_lsb(knights)) {
		uint64_t from64 = lsb(knights);
		int from = bsf(knights);
		bool revealed_check = revealed_bcheck(mg, from64, 0)
		    || revealed_rcheck(mg, from64, 0);
		uint64_t dsts = knight_pattern(from) & mg->dst_mask;

		for (; is_nonempty(dsts); dsts = reset_lsb(dsts)) {
			uint64_t to64 = lsb(dsts);
			int to = bsf(to64);
			bool check = revealed_check;
			check |= is_nonempty(to64 & mg->opp_king_knight_reach);

			*mg->moves++ = create_move_g(from, to, knight,
			    mg->pos->board[to], check);
		}
	}
}

static void
gen_sliding_moves(struct move_gen *mg, int from, uint64_t to_map,
		int piece,
		bool revealed_check, uint64_t opp_king_reach_map)
{
	for (; is_nonempty(to_map); to_map = reset_lsb(to_map)) {
		uint64_t to64 = lsb(to_map);
		bool check = revealed_check
		    || is_nonempty(to64 & opp_king_reach_map);
		int to = bsf(to64);

		*mg->moves++ = create_move_g(from, to, piece,
		    mg->pos->board[to], check);
	}
}


static void
gen_bishop_moves(struct move_gen *mg)
{
	uint64_t bishops = mg->pos->map[bishop] & ~mg->pos->rpin_map;

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		uint64_t from64 = lsb(bishops);
		int from = bsf(bishops);
		uint64_t dst_map = mg->dst_mask
		    & sliding_map(mg->pos->occupied, bishop_magics + from);

		if (is_nonempty(from64 & mg->pos->bpin_map))
			dst_map &= mg->pos->bpin_map;

		bool revealed_check = revealed_rcheck(mg, from64, 0);

		gen_sliding_moves(mg, from, dst_map, bishop,
		    revealed_check, mg->opp_king_bishop_reach);
	}
}

static void
gen_rook_moves(struct move_gen *mg)
{
	uint64_t rooks = mg->pos->map[rook] & ~mg->pos->bpin_map;

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		uint64_t from64 = lsb(rooks);
		int from = bsf(rooks);
		uint64_t dst_map = mg->dst_mask
		    & sliding_map(mg->pos->occupied, rook_magics + from);

		if (is_nonempty(from64 & mg->pos->rpin_map))
			dst_map &= mg->pos->rpin_map;

		bool revealed_check = revealed_bcheck(mg, from64, 0);

		gen_sliding_moves(mg, from, dst_map, rook,
		    revealed_check, mg->opp_king_rook_reach);
	}
}

static void
gen_queen_moves(struct move_gen *mg)
{
	uint64_t queens = mg->pos->map[queen];

	for (; is_nonempty(queens); queens = reset_lsb(queens)) {
		int64_t from64 = lsb(queens);
		int from = bsf(from64);
		uint64_t bdst_map = EMPTY;
		uint64_t rdst_map = EMPTY;

		if (is_empty(from64 & mg->pos->bpin_map))
			rdst_map = sliding_map(mg->pos->occupied,
			    rook_magics + from);

		if (is_empty(from64 & mg->pos->rpin_map))
			bdst_map = sliding_map(mg->pos->occupied,
			    bishop_magics + from);

		if (is_nonempty(from64 & mg->pos->rpin_map))
			rdst_map &= mg->pos->rpin_map;
		if (is_nonempty(from64 & mg->pos->bpin_map))
			bdst_map &= mg->pos->bpin_map;

		uint64_t dst_map = (bdst_map | rdst_map) & mg->dst_mask;

		gen_sliding_moves(mg, from, dst_map, queen, false,
		    mg->opp_king_rook_reach | mg->opp_king_bishop_reach);
	}
}

static void
gen_bishop_moves_nopin(struct move_gen *mg)
{
	uint64_t bishops = mg->pos->map[bishop];

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		uint64_t from64 = lsb(bishops);
		int from = bsf(bishops);
		uint64_t dst_map = mg->dst_mask
		    & sliding_map(mg->pos->occupied, bishop_magics + from);

		bool revealed_check = revealed_rcheck(mg, from64, 0);

		gen_sliding_moves(mg, from, dst_map, bishop,
		    revealed_check, mg->opp_king_bishop_reach);
	}
}

static void
gen_rook_moves_nopin(struct move_gen *mg)
{
	uint64_t rooks = mg->pos->map[rook];

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		uint64_t from64 = lsb(rooks);
		int from = bsf(rooks);
		uint64_t dst_map = mg->dst_mask
		    & sliding_map(mg->pos->occupied, rook_magics + from);

		bool revealed_check = revealed_bcheck(mg, from64, 0);

		gen_sliding_moves(mg, from, dst_map, rook,
		    revealed_check, mg->opp_king_rook_reach);
	}
}

static void
gen_queen_moves_nopin(struct move_gen *mg)
{
	uint64_t queens = mg->pos->map[queen];

	for (; is_nonempty(queens); queens = reset_lsb(queens)) {
		uint64_t from64 = lsb(queens);
		int from = bsf(from64);
		uint64_t dst_map = sliding_map(mg->pos->occupied, rook_magics + from);
		dst_map |= sliding_map(mg->pos->occupied, bishop_magics + from);
		dst_map &= mg->dst_mask;

		gen_sliding_moves(mg, from, dst_map, queen, false,
		    mg->opp_king_rook_reach | mg->opp_king_bishop_reach);
	}
}

static void
gen_moves_general(struct move_gen *mg)
{
	gen_knight_moves(mg);
	gen_rook_moves(mg);
	gen_bishop_moves(mg);
	gen_queen_moves(mg);
	gen_pawn_captures(mg);
	gen_promotions(mg);
	gen_en_passant(mg);
}

static void
gen_moves_general_nopin(struct move_gen *mg)
{
	gen_knight_moves(mg);
	gen_rook_moves_nopin(mg);
	gen_bishop_moves_nopin(mg);
	gen_queen_moves_nopin(mg);
	gen_pawn_captures_nopin(mg);
	gen_promotions_nopin(mg);
	gen_pawn_pushes_nopin(mg);
	gen_en_passant_nopin(mg);
}

static void
init_data(struct move_gen *mg, const struct position *pos,
		move moves[static MOVE_ARRAY_LENGTH])
{
	uint64_t k = pos->map[opponent_king];
	mg->pos = pos;
	mg->moves = moves;
	mg->opp_ki = bsf(k);
	mg->bishop_movers = pos->map[bishop];
	mg->bishop_movers |= pos->map[queen];
	mg->rook_movers = pos->map[rook];
	mg->rook_movers |= pos->map[queen];
	mg->opp_king_pawn_reach = pawn_reach_south(k);
	mg->opp_king_knight_reach = knight_pattern(mg->opp_ki);
	mg->opp_king_bishop_reach =
	    sliding_map(pos->occupied, bishop_magics + mg->opp_ki);
	mg->opp_king_rook_reach =
	    sliding_map(pos->occupied, rook_magics + mg->opp_ki);
}


unsigned
gen_moves(const struct position *pos, move moves[static MOVE_ARRAY_LENGTH])
{
	struct move_gen mg[1];

	init_data(mg, pos, moves);
	if (popcnt(pos_king_attackers(pos)) <= 1) {
		if (is_in_check(pos)) {
			mg->dst_mask = pos->king_attack_map;
		}
		else {
			mg->dst_mask = ~pos->map[0];
			gen_castle_king_side(mg);
			gen_castle_queen_side(mg);
		}
		if (is_empty(pos->bpin_map) && is_empty(pos->rpin_map)) {
			gen_moves_general_nopin(mg);
		}
		else {
			gen_moves_general(mg);
			gen_pawn_pushes(mg);
		}
	}
	gen_king_moves(mg, ~pos->map[0]);
	*mg->moves = 0;

	return mg->moves - moves;
}

unsigned
gen_captures(const struct position *pos,
		move moves[static MOVE_ARRAY_LENGTH])
{
	struct move_gen mg[1];

	init_data(mg, pos, moves);
	mg->dst_mask = pos->map[1];
	if (is_nonempty(pos->attack[0] & mg->dst_mask)) {
		gen_moves_general(mg);
		gen_king_moves(mg, pos->map[1]);
	}
	gen_en_passant(mg);
	*mg->moves = 0;

	return mg->moves - moves;
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
