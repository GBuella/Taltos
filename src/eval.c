
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "bitboard.h"
#include "position.h"
#include "search.h"
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
	rook_on_open_file = 16,
	rook_on_half_open_file = 18,
	rook_pair = 10,
	pawn_on_center = 2,
	pawn_attacks_center = 3,
	pawn_attacks_center4 = 3,
	isolated_pawn = -10,
	blocked_pawn = -11,
	knight_center = 2,
	knight_center4 = 1,
	pawn_chain = 3,
	knight_attack_center = 1,
	bishop_attack_center = 1,
	knight_outpost = 10,
	kings_pawn = 5,
	castled_king = 2,
	king_ring_sliding_attacked = -4,
	passed_pawn = 16,
	tempo = 3
};

static int
basic_mobility(const struct position *pos)
{
	int value = 0;

	/*
	 * One centipawn for each square reachable by any piece
	 * other than pawns.
	 */
	value += popcnt(pos->attack[0]);
	value -= popcnt(pos->attack[1]);

	/*
	 * One more centipawn for each empty square - not attacked by other
	 * players pawn - reachable by any piece, other than by pawn.
	 */
	value += popcnt(pos->attack[0]
	    & ~(pos->occupied | pos->attack[opponent_pawn]));

	value -= popcnt(pos->attack[1]
	    & ~(pos->occupied | pos->attack[pawn]));

	/*
	 * Two more centipawn for each empty square reachable by any piece
	 * other than pawns, that are not attacked by the other player.
	 * ( One point in case of king )
	 */

	uint64_t dst = ~(pos->occupied | pos->attack[1]);
	value += 2 * popcnt(pos->attack[queen] & dst);
	value += 2 * popcnt(pos->attack[rook] & dst);
	value += 2 * popcnt(pos->attack[bishop] & dst);
	value += 2 * popcnt(pos->attack[knight] & dst);
	value += popcnt(pos->attack[king] & dst);

	dst = ~(pos->occupied | pos->attack[0]);
	value -= 2 * popcnt(pos->attack[opponent_queen] & dst);
	value -= 2 * popcnt(pos->attack[opponent_rook] & dst);
	value -= 2 * popcnt(pos->attack[opponent_bishop] & dst);
	value -= 2 * popcnt(pos->attack[opponent_knight] & dst);
	value -= popcnt(pos->attack[opponent_king] & dst);

	return value;
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
		int value = (popcnt(pos->occupied) - 1) / 8;
		value += popcnt(pos->map[queen] | pos->map[opponent_queen]);
		return value;
	}
	else return 0;
}

static int
king_fortress(const struct position *pos)
{
	int value = 0;
	if (is_nonempty(pos->map[king] & (RANK_1|RANK_2))) {
		/*
		 * Bonus for castled king to encourage castling,
		 * or at least not losing castling rights.
		 */
		if (is_nonempty(pos->map[king] & (SQ_G1|SQ_H1))
		    && pos->board[sq_h1] != rook)
			value += castled_king;
		else if (is_nonempty(pos->map[king] & (SQ_A1|SQ_C1|SQ_B1))
		    && pos->board[sq_a1] != rook)
			value += castled_king;
		else
			value += pos->cr_king_side + pos->cr_queen_side;

		/* Count pawns around the king */
		uint64_t pawns = pos->attack[king] & pos->map[pawn];
		value += (kings_pawn / 2) * popcnt(pawns);
		value += kings_pawn * popcnt(pawns & ~pos->attack[1]);

		/*
		 * Penalty for squares around the king being attacked by
		 * other player
		 */
		value += king_ring_sliding_attacked *
		    popcnt(pos->attack[king] & pos->sliding_attacks[1]);
		value += king_ring_sliding_attacked *
		    popcnt(pos->attack[king] & pos->attack[opponent_queen]);

		/* shield - up to three squares in front of the king */
		uint64_t shield = north_of(pos->map[king])
		    | north_of(west_of(pos->map[king] & ~FILE_H))
		    | north_of(east_of(pos->map[king] & ~FILE_A));

		// Penalty for not having any pawn on a file around the king
		value -= popcnt(shield & pos->half_open_files[0]);

		/*
		 * Even more bonus for having a pawn in front of the king,
		 * vs. just left or right to the king.
		 */
		pawns = north_of(shield) & pos->map[pawn];
		value += kings_pawn * popcnt(pawns);
	}

	if (is_nonempty(pos->map[opponent_king] & (RANK_8|RANK_7))) {
		if (is_nonempty(pos->map[opponent_king] & (SQ_G8|SQ_H8))
		    && pos->board[sq_h8] != rook)
			value -= castled_king;
		else if (is_nonempty(pos->map[opponent_king]
		    & (SQ_A8|SQ_B8|SQ_C8))
		    && pos->board[sq_a8] != rook)
			value -= castled_king;
		else
			value -= pos->cr_opponent_king_side
			    + pos->cr_opponent_queen_side;
		uint64_t pawns = pos->attack[opponent_king]
		    & pos->map[opponent_pawn];
		value -= (kings_pawn / 2) * popcnt(pawns);
		value -= kings_pawn * popcnt(pawns & ~pos->attack[0]);
		value -= king_ring_sliding_attacked *
		    popcnt(pos->attack[opponent_king]
		    & pos->sliding_attacks[0]);
		value -= king_ring_sliding_attacked *
		    popcnt(pos->attack[opponent_king] & pos->attack[queen]);
		uint64_t shield = south_of(pos->map[opponent_king])
		    | south_of(west_of(pos->map[opponent_king] & ~FILE_H))
		    | south_of(east_of(pos->map[opponent_king] & ~FILE_A));

		value += popcnt(shield & pos->half_open_files[1]);
		pawns = south_of(shield) & pos->map[opponent_pawn];
		value -= kings_pawn * popcnt(pawns);
	}

	return value;
}

static int
center_control(const struct position *pos)
{
	int value;

	value = pawn_attacks_center *
	    (popcnt(pos->attack[pawn] & CENTER_SQ)
	    - popcnt(pos->attack[opponent_pawn] & CENTER_SQ));
	value = pawn_attacks_center4 *
	    (popcnt(pos->attack[pawn] & CENTER4_SQ)
	    - popcnt(pos->attack[opponent_pawn] & CENTER4_SQ));
	value += pawn_on_center *
	    (popcnt(pos->map[pawn] & CENTER_SQ)
	    - popcnt(pos->attack[opponent_pawn] & CENTER_SQ));
	value += knight_attack_center *
	    (popcnt(pos->attack[knight] & CENTER_SQ)
	    - popcnt(pos->attack[opponent_knight] & CENTER_SQ));
	value += bishop_attack_center *
	    (popcnt(pos->attack[bishop] & CENTER_SQ)
	    - popcnt(pos->attack[opponent_bishop] & CENTER_SQ));
	value += knight_center *
	    (popcnt(pos->map[knight] & CENTER_SQ)
	    - popcnt(pos->map[opponent_knight] & CENTER_SQ));
	value += knight_center4 *
	    (popcnt(pos->map[knight] & CENTER4_SQ)
	    - popcnt(pos->map[opponent_knight] & CENTER4_SQ));

	return value;
}

static int
rook_placement(const struct position *pos)
{
	int value;
	uint64_t open_files = pos->half_open_files[0] & pos->half_open_files[1];

	value = popcnt(open_files & pos->map[rook]);
	value -= popcnt(open_files & pos->map[opponent_rook]);
	value *= rook_on_open_file;
	open_files &= ~(FILE_A | FILE_H);
	value += popcnt(open_files & pos->map[rook]);
	value -= popcnt(open_files & pos->map[opponent_rook]);

	value += rook_on_half_open_file *
	    (popcnt(pos->half_open_files[0] & pos->map[rook])
	    - popcnt(pos->half_open_files[1] & pos->map[opponent_rook]));
	value += rook_pair *
	    (popcnt(pos->half_open_files[0] & pos->map[rook]
	    & south_of(kogge_stone_south(pos->map[rook]))));
	value -= rook_pair *
	    (popcnt(pos->half_open_files[1] & pos->map[opponent_rook]
	    & south_of(kogge_stone_south(pos->map[opponent_rook]))));

	return value;
}

static int
pawn_structure(const struct position *pos)
{
	int value;

	value = pawn_chain *
	    (popcnt(pos->map[pawn] & pos->attack[pawn])
	    - popcnt(pos->map[opponent_pawn] & pos->attack[opponent_pawn]));

	value += isolated_pawn *
	    popcnt(pos->map[pawn]
	    & east_of(pos->half_open_files[0] & ~FILE_H));
	value += isolated_pawn *
	    popcnt(pos->map[pawn]
	    & west_of(pos->half_open_files[0] & ~FILE_A));
	value -= isolated_pawn *
	    popcnt(pos->map[opponent_pawn]
	    & east_of(pos->half_open_files[1] & ~FILE_H));
	value -= isolated_pawn *
	    popcnt(pos->map[opponent_pawn]
	    & west_of(pos->half_open_files[1] & ~FILE_A));

	value += blocked_pawn *
	    (popcnt(north_of(pos->map[pawn]) & pos->map[1])
	    - popcnt(south_of(pos->map[opponent_pawn]) & pos->map[0]));
	value += blocked_pawn *
	    popcnt(pos->map[pawn] & north_of(pos->map[pawn]));
	value -= blocked_pawn *
	    popcnt(pos->map[opponent_pawn] & north_of(pos->map[opponent_pawn]));

	return value;
}

static int
knight_placement(const struct position *pos)
{
	int value = 0;
	uint64_t outpost = pos->map[knight];
	uint64_t opponent_outpost = pos->map[opponent_knight];

	outpost &= pos->attack[pawn];
	opponent_outpost &= pos->attack[opponent_pawn];
	outpost &= ~pos->pawn_attack_reach[1];
	opponent_outpost &= ~pos->pawn_attack_reach[0];
	outpost &= ~RANK_8;
	opponent_outpost &= ~RANK_1;

	value += knight_outpost * popcnt(outpost);
	value -= knight_outpost * popcnt(opponent_outpost);

	value += (knight_outpost / 2) * popcnt(outpost & CENTER4_SQ);
	value -= (knight_outpost / 2) * popcnt(opponent_outpost & CENTER4_SQ);

	return value;
}

static int
passed_pawns(const struct position *pos)
{
	int value;
	uint64_t pawns;
	uint64_t danger;

	pawns = north_of(pos->map[pawn] & RANK_7) & ~pos->occupied;
	value = popcnt(pawns) * passed_pawn;

	pawns = south_of(pos->map[opponent_pawn] & RANK_2) & ~pos->occupied;
	value -= popcnt(pawns) * passed_pawn;

	pawns = pos->map[pawn] & (RANK_7 | RANK_6 | RANK_5 | RANK_4);
	danger = pos->map[opponent_pawn] | pos->attack[opponent_pawn];
	pawns &= ~kogge_stone_south(danger);

	value += (passed_pawn / 2) * popcnt(pawns);
	value += (passed_pawn / 4) * popcnt(pawns & pos->attack[pawn]);

	uint64_t path = kogge_stone_north(pawns) & ~pos->occupied;
	value += (passed_pawn / 2) * popcnt(path & RANK_8);

	pawns = pos->map[opponent_pawn] & (RANK_2 | RANK_3 | RANK_4 | RANK_5);
	danger = pos->map[pawn] | pos->attack[pawn];
	pawns &= ~kogge_stone_north(danger);

	value -= (passed_pawn / 2) * popcnt(pawns);
	value -= (passed_pawn / 4) * popcnt(pawns & pos->attack[opponent_pawn]);

	path = kogge_stone_south(pawns) & ~pos->occupied;
	value -= (passed_pawn / 2) * popcnt(path & RANK_8);

	return value;
}

int
eval(const struct position *pos)
{
	int value;

	value = pos->material_value;
	value += basic_mobility(pos);
	value += rook_placement(pos);
	value += pawn_structure(pos);
	value += knight_placement(pos);
	int mg_factor = middle_game(pos);
	if (mg_factor != 0)
		value += mg_factor *
		    (center_control(pos) + king_fortress(pos));
	int eg_factor = end_game(pos);
	value += (eg_factor + 1) * passed_pawns(pos);
	invariant(value_bounds(value));

	return value + tempo;
}


struct eval_factors
compute_eval_factors(const struct position *pos)
{
	struct eval_factors ef;

	ef.material = pos->material_value;
	ef.basic_mobility = basic_mobility(pos);
	ef.end_game = end_game(pos);
	ef.middle_game = middle_game(pos);
	ef.center_control = center_control(pos);
	ef.pawn_structure = pawn_structure(pos);
	ef.passed_pawns = passed_pawns(pos);
	ef.king_fortress = king_fortress(pos);
	ef.rook_placement = rook_placement(pos);
	ef.knight_placement = knight_placement(pos);
	ef.center_control = center_control(pos);
	return ef;
}
