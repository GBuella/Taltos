
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
	uint64_t opp_bishop_movers;
	uint64_t rook_movers;
	uint64_t opp_rook_movers;
	uint64_t opp_king_pawn_reach;
	uint64_t opp_king_knight_reach;
	uint64_t opp_king_bishop_reach;
	uint64_t opp_king_rook_reach;
};

static uint64_t
potential_revealed_attacker(const struct move_gen *mg, int from)
{
	int ray_index = ray_dir_table[mg->opp_ki][from];
	if (ray_index == 0xff)
		return EMPTY;

	if (is_nonempty(ray_table[mg->opp_ki][from] & mg->pos->occupied))
		return EMPTY;

	uint64_t ray = mg->pos->rays[ray_index][from];
	if (ray_index < 4)
		return ray & mg->rook_movers;
	else
		return ray & mg->bishop_movers;
}

static uint64_t
revealed_check_to_map(const struct move_gen *mg, int from)
{
	uint64_t attacker = potential_revealed_attacker(mg, from);

	if (is_nonempty(attacker))
		return ~ray_table[mg->opp_ki][bsf(attacker)];
	else
		return EMPTY;
}

static uint64_t
pinner_map(const struct move_gen *mg, int from)
{
	int ki = mg->pos->king_index;

	int ray_index = ray_dir_table[ki][from];
	if (ray_index == 0xff)
		return EMPTY;

	if (is_nonempty(ray_table[ki][from] & mg->pos->occupied))
		return EMPTY;

	uint64_t ray = mg->pos->rays[ray_index][from];
	uint64_t pinner = ray & mg->pos->map[opponent_queen];
	if (is_rook_ray_index(ray_index))
		pinner |= ray & mg->pos->map[opponent_rook];
	else
		pinner |= ray & mg->pos->map[opponent_bishop];

	assert(is_empty(pinner) || is_singular(pinner));

	return pinner;
}

static uint64_t
nopin_map(const struct move_gen *mg, int from)
{
	uint64_t pinner = pinner_map(mg, from);

	if (is_nonempty(pinner))
		return pinner | ray_table[mg->pos->king_index][bsf(pinner)];
	else
		return UNIVERSE;
}

static void
ep_check_kings_horizontally(const struct move_gen *mg, int from,
				bool *pin, bool *check)
{
	int wpiece;
	int epiece;
	if (mg->pos->ep_index == (uint64_t)(from + WEST)) {
		epiece = from;
		wpiece = from + WEST;
	}
	else {
		epiece = from + EAST;
		wpiece = from;
	}

	uint64_t east_end = mg->pos->rays[pr_east][epiece] & mg->pos->occupied;
	uint64_t west_end = mg->pos->rays[pr_west][wpiece] & mg->pos->occupied;

	if (is_nonempty(east_end & mg->pos->map[king]))
		*pin = is_nonempty(west_end & mg->opp_rook_movers);
	else if (is_nonempty(west_end & mg->pos->map[king]))
		*pin = is_nonempty(east_end & mg->opp_rook_movers);
	else if (is_nonempty(east_end & mg->pos->map[opponent_king]))
		*check |= is_nonempty(west_end & mg->rook_movers);
	else if (is_nonempty(west_end & mg->pos->map[opponent_king]))
		*check |= is_nonempty(east_end & mg->rook_movers);
}

static void
gen_king_moves(struct move_gen *mg, uint64_t dst_mask)
{
	int from = mg->pos->king_index;
	uint64_t revealed_checks = revealed_check_to_map(mg, from);
	uint64_t dsts = mg->pos->attack[king];
	dsts &= dst_mask & ~mg->pos->king_danger_map;

	for (; is_nonempty(dsts); dsts = reset_lsb(dsts)) {
		uint64_t to64 = lsb(dsts);
		int to = bsf(to64);
		int captured = mg->pos->board[to];
		bool check = is_nonempty(revealed_checks & to64);

		*mg->moves++ = create_move_g(from, to, king, captured, check);
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

	bool check = is_nonempty(to64 & mg->opp_king_pawn_reach);

	uint64_t attackers = pawn_reach_south(to64) & mg->pos->map[pawn];
	if (is_empty(attackers))
		return;

	if (is_singular(attackers)) {
		/*
		 * Horizontal pins and revealed checks are only possible here,
		 * if a single pawn can make the en-passant capture.
		 *
		 * |        |     |        |
		 * |        | --> |    P   |
		 * | r  pP K|     | r     K|
		 *                   |
		 *                   check!
		 *
		 * With two pawns, these are not possible, as the other pawn
		 * stay there, and blocks:
		 *
		 * |        |     |        |
		 * |        | --> |    P   |
		 * | r PpP K|     | r P   K|
		 *
		 */
		bool pin = false;
		ep_check_kings_horizontally(mg, bsf(attackers), &pin, &check);
		if (pin)
			return;
	}

	for (; is_nonempty(attackers); attackers = reset_lsb(attackers)) {
		int from = bsf(attackers);
		if (is_empty(nopin_map(mg, from) & to64))
			continue;

		uint64_t reveal_diagonal = revealed_check_to_map(mg, from);
		bool gives_check = check | is_nonempty(reveal_diagonal & to64);

		*mg->moves++ = create_move_t(from, bsf(to64), mt_en_passant,
		    pawn, pawn, gives_check);
	}
}

static bool
promotion_rcheck(const struct move_gen *mg, int from, uint64_t to64)
{
	if (is_nonempty(to64 & mg->opp_king_rook_reach))
		return true;

	uint64_t from64 = bit64(from);

	if (to64 != north_of(from64))
		return false;

	if (is_empty(from64 & mg->opp_king_rook_reach))
		return false;

	if (south_of(from64) == mg->pos->map[opponent_king])
		return true;

	return is_nonempty(south_of(from64) & mg->opp_king_rook_reach);
}

static bool
promotion_bcheck(const struct move_gen *mg, int from, uint64_t to64)
{
	if (is_nonempty(to64 & mg->opp_king_bishop_reach))
		return true;

	uint64_t from64 = bit64(from);

	if (to64 == north_of(from64))
		return false;

	if (is_empty(from64 & mg->opp_king_bishop_reach))
		return false;

	int ray_index = ray_dir_table[mg->opp_ki][from];
	if (ray_index == 0xff)
		return false;

	return mg->pos->rays[ray_index][from] == to64;
}

static void
gen_promotions(struct move_gen *mg, int from, uint64_t dsts,
		uint64_t revealed_checks)
{
	bool check = is_nonempty(revealed_checks & dsts);
	bool rcheck = check;
	bool bcheck = check;
	bool kncheck = check;

	for (; is_nonempty(dsts); dsts = reset_lsb(dsts)) {
		uint64_t to64 = lsb(dsts);
		int to = bsf(to64);
		if (!check) {
			kncheck = is_nonempty(to64 & mg->opp_king_knight_reach);
			rcheck = promotion_rcheck(mg, from, to64);
			bcheck = promotion_bcheck(mg, from, to64);
		}

		*mg->moves++ = create_move_t(from, to, mt_promotion, queen,
				mg->pos->board[to], rcheck || bcheck);
		*mg->moves++ = create_move_t(from, to, mt_promotion, knight,
				mg->pos->board[to], kncheck);
		*mg->moves++ = create_move_t(from, to, mt_promotion, rook,
				mg->pos->board[to], rcheck);
		*mg->moves++ = create_move_t(from, to, mt_promotion, bishop,
				mg->pos->board[to], bcheck);
	}
}

static void
gen_piece_moves(struct move_gen *mg, int from, int piece,
		uint64_t dsts, uint64_t checks)
{
	for (; is_nonempty(dsts); dsts = reset_lsb(dsts)) {
		uint64_t to64 = lsb(dsts);
		int to = bsf(to64);
		bool check = is_nonempty(checks & to64);

		*mg->moves++ = create_move_g(from, to, piece,
				mg->pos->board[to], check);
	}
}

static void
gen_knight_moves(struct move_gen *mg)
{
	uint64_t knights = mg->pos->map[knight];

	for (; is_nonempty(knights); knights = reset_lsb(knights)) {
		int from = bsf(knights);
		if (is_nonempty(pinner_map(mg, from)))
			continue;

		uint64_t dsts = knight_pattern(from) & mg->dst_mask;
		uint64_t checks;
		if (is_nonempty(potential_revealed_attacker(mg, from)))
			checks = dsts;
		else
			checks = mg->opp_king_knight_reach;

		gen_piece_moves(mg, from, knight, dsts, checks);
	}
}

static void
gen_bishop_moves(struct move_gen *mg)
{
	uint64_t bishops = mg->pos->map[bishop];

	for (; is_nonempty(bishops); bishops = reset_lsb(bishops)) {
		int from = bsf(bishops);
		uint64_t dst_map = mg->pos->rays_bishops[from] & mg->dst_mask;
		dst_map &= nopin_map(mg, from);
		uint64_t check_map = revealed_check_to_map(mg, from);
		check_map |= mg->opp_king_bishop_reach;

		gen_piece_moves(mg, from, bishop, dst_map, check_map);
	}
}

static void
gen_rook_moves(struct move_gen *mg)
{
	uint64_t rooks = mg->pos->map[rook];

	for (; is_nonempty(rooks); rooks = reset_lsb(rooks)) {
		int from = bsf(rooks);
		uint64_t dst_map = mg->pos->rays_rooks[from] & mg->dst_mask;
		dst_map &= nopin_map(mg, from);
		uint64_t check_map = revealed_check_to_map(mg, from);
		check_map |= mg->opp_king_rook_reach;

		gen_piece_moves(mg, from, rook, dst_map, check_map);
	}
}

static void
gen_queen_moves(struct move_gen *mg)
{
	uint64_t queens = mg->pos->map[queen];

	for (; is_nonempty(queens); queens = reset_lsb(queens)) {
		int from = bsf(queens);
		uint64_t dst_map;
		dst_map = mg->pos->rays_bishops[from];
		dst_map |= mg->pos->rays_rooks[from];
		dst_map &= mg->dst_mask;
		dst_map &= nopin_map(mg, from);
		uint64_t check_map = mg->opp_king_rook_reach;
		check_map |= mg->opp_king_bishop_reach;

		gen_piece_moves(mg, from, queen, dst_map, check_map);
	}
}

static void
gen_pawn_double_push(struct move_gen *mg, int from, uint64_t checks, uint64_t nopin)
{
	uint64_t to64 = north_of(bit64(from));
	if (is_nonempty(to64 & mg->pos->occupied))
		return;

	if (is_empty(to64 & nopin))
		return;

	to64 = north_of(to64);
	if (is_nonempty(to64 & mg->pos->occupied))
		return;
	if (is_empty(to64 & mg->dst_mask))
		return;

	bool check = is_nonempty(checks & to64);
	*mg->moves++ =
	    create_move_t(from, bsf(to64), mt_pawn_double_push, pawn, 0, check);
}

static void
gen_pawn_moves(struct move_gen *mg)
{
	uint64_t pawns = mg->pos->map[pawn];

	for (; is_nonempty(pawns); pawns = reset_lsb(pawns)) {
		uint64_t from64 = lsb(pawns);
		int from = bsf(pawns);

		uint64_t nopin = nopin_map(mg, from);
		uint64_t dst_map = north_of(from64) & ~mg->pos->occupied;
		dst_map |= pawn_reach_north(from64) & mg->pos->map[1];
		dst_map &= mg->dst_mask;
		dst_map &= nopin;

		uint64_t checks = revealed_check_to_map(mg, from);

		if (is_empty(from64 & RANK_7)) {
			checks |= mg->opp_king_pawn_reach;
			gen_piece_moves(mg, from, pawn, dst_map, checks);
			if (ind_rank(from) == rank_2)
				gen_pawn_double_push(mg, from, checks, nopin);
		}
		else {
			gen_promotions(mg, from, dst_map, checks);
		}
	}
}

static void
gen_moves_general(struct move_gen *mg)
{
	gen_knight_moves(mg);
	gen_rook_moves(mg);
	gen_bishop_moves(mg);
	gen_queen_moves(mg);
	gen_pawn_moves(mg);
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
	mg->opp_bishop_movers = pos->map[opponent_bishop];
	mg->opp_bishop_movers |= pos->map[opponent_queen];
	mg->rook_movers = pos->map[rook];
	mg->rook_movers |= pos->map[queen];
	mg->opp_rook_movers = pos->map[opponent_rook];
	mg->opp_rook_movers |= pos->map[opponent_queen];
	mg->opp_king_pawn_reach = pawn_reach_south(k);
	mg->opp_king_knight_reach = knight_pattern(mg->opp_ki);
	mg->opp_king_bishop_reach = pos->rays_bishops[mg->opp_ki];
	mg->opp_king_rook_reach = pos->rays_rooks[mg->opp_ki];
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
		gen_moves_general(mg);
		gen_en_passant(mg);
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
	const uint8_t *byte_lookup_table, const uint64_t *table)
{
	dst->mask = raw_info[0];
	dst->multiplier = raw_info[1];
	dst->shift = (int)(raw_info[2] & 0xff);
	dst->attack_table = table + raw_info[3];
	dst->attack_index_table = byte_lookup_table + (raw_info[2]>>8);
}

void
init_move_gen(void)
{
	static const int magic_block = 4;
	for (int i = 0; i < 64; ++i) {
		init_sliding_move_magics(rook_magics + i,
		    rook_magics_raw + magic_block * i,
		    rook_attack_index8, rook_magic_attacks);
		init_sliding_move_magics(bishop_magics + i,
		    bishop_magics_raw + magic_block * i,
		    bishop_attack_index8, bishop_magic_attacks);
	}
}
