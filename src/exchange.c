
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <stdbool.h>

#include "exchange.h"
#include "position.h"
#include "eval.h"

struct SEE_state {
	const struct position *pos;
	bool check_pins;
	int side;
	uint64_t dst;
	uint64_t occ;
	uint64_t bishop_reach;
	uint64_t rook_reach;
};

static uint64_t
player_pieces(const struct SEE_state *state, int piece)
{
	return state->pos->map[piece + state->side];
}

static uint64_t
opp_pieces(const struct SEE_state *state, int piece)
{
	return state->pos->map[piece + !state->side];
}

static uint64_t
pawn_threats_player(const struct SEE_state *state)
{
	if (state->side == 0)
		return pawn_reach_south(state->dst);
	else
		return pawn_reach_north(state->dst);
}

static uint64_t
pawn_threats_opponent(const struct SEE_state *state)
{
	if (state->side == 0)
		return pawn_reach_north(state->dst);
	else
		return pawn_reach_south(state->dst);
}

static uint64_t
player_pawns(const struct SEE_state *state)
{
	return player_pieces(state, pawn);
}

static uint64_t
opp_pawns(const struct SEE_state *state)
{
	return opp_pieces(state, pawn);
}

static uint64_t
player_knights(const struct SEE_state *state)
{
	return player_pieces(state, knight);
}

static uint64_t
opp_knights(const struct SEE_state *state)
{
	return opp_pieces(state, knight);
}

static uint64_t
player_bishops(const struct SEE_state *state)
{
	return player_pieces(state, bishop);
}

static uint64_t
opp_bishops(const struct SEE_state *state)
{
	return opp_pieces(state, bishop);
}

static uint64_t
player_rooks(const struct SEE_state *state)
{
	return player_pieces(state, rook);
}

static uint64_t
opp_rooks(const struct SEE_state *state)
{
	return opp_pieces(state, rook);
}

static uint64_t
player_queens(const struct SEE_state *state)
{
	return player_pieces(state, queen);
}

static uint64_t
opp_queens(const struct SEE_state *state)
{
	return opp_pieces(state, queen);
}

static uint64_t
get_pinner(const struct SEE_state *state, uint64_t piece)
{
	uint64_t king_map = player_pieces(state, king);
	int ki = bsf(king_map);
	int i = bsf(piece);

	if (popcnt(ray_table[ki][i] & state->occ) > 2)
		return 0;

	uint64_t dir_ray = dir_ray_table[ki][i] & state->occ;

	dir_ray &= ~piece;
	dir_ray &= ~king_map;

	if (is_empty(dir_ray))
		return 0;

	uint64_t pinner;

	if (ki > i)
		pinner = msb(dir_ray);
	else
		pinner = lsb(dir_ray);

	pinner &= state->occ;

	if (is_nonempty(pinner & opp_queens(state)))
		return pinner;

	if (is_nonempty(pinner & rook_pattern_table[ki]))
		pinner &= opp_rooks(state);
	else
		pinner &= opp_bishops(state);

	return pinner;
}

static void
compute_bishop_reach(struct SEE_state *state)
{
	state->bishop_reach = sliding_map(state->occ,
	    bishop_magics + bsf(state->dst));
}

static void
compute_rook_reach(struct SEE_state *state)
{
	state->rook_reach = sliding_map(state->occ,
	    rook_magics + bsf(state->dst));
}

static uint64_t
side_lsb(struct SEE_state *state, uint64_t map)
{
	if (state->side == 0)
		return lsb(map);
	else
		return msb(map);
}

static uint64_t
check_pieces(struct SEE_state *state, uint64_t pieces)
{
	pieces &= state->occ;

	if (!state->check_pins)
		return side_lsb(state, pieces);

	while (is_nonempty(pieces)) {
		uint64_t piece = side_lsb(state, pieces);
		uint64_t pinner = get_pinner(state, piece);

		if (is_empty(pinner) || pinner == state->dst)
			return piece;

		pieces &= ~piece;
	}

	return 0;
}

static uint64_t
SEE_pawn(struct SEE_state *state)
{
	uint64_t pawns;

	pawns = pawn_threats_player(state);
	pawns &= player_pawns(state);
	return check_pieces(state, pawns);
}

static uint64_t
SEE_knight(struct SEE_state *state)
{
	uint64_t knights;

	knights = knight_pattern(bsf(state->dst)) & player_knights(state);
	return check_pieces(state, knights);
}

static uint64_t
SEE_bishop(struct SEE_state *state)
{
	uint64_t bishops;

	compute_bishop_reach(state);
	bishops = state->bishop_reach & player_bishops(state);
	return check_pieces(state, bishops);
}

static uint64_t
SEE_rook(struct SEE_state *state)
{
	uint64_t rooks;

	compute_rook_reach(state);
	rooks = state->rook_reach & player_rooks(state);
	return check_pieces(state, rooks);
}

static uint64_t
SEE_queen(struct SEE_state *state)
{
	uint64_t queens;
	uint64_t attacker;

	compute_rook_reach(state);
	queens = state->rook_reach & player_queens(state);
	attacker = check_pieces(state, queens);
	if (is_nonempty(attacker))
		return attacker;

	compute_bishop_reach(state);
	queens = state->bishop_reach & player_queens(state);
	return check_pieces(state, queens);
}

static uint64_t
SEE_king(struct SEE_state *state)
{
	if (is_empty(state->pos->attack[king + state->side] & state->dst))
		return 0;

	uint64_t attacks;

	attacks = pawn_threats_opponent(state);
	attacks &= state->occ & opp_pawns(state);
	if (is_nonempty(attacks))
		return 0;

	attacks = knight_pattern(bsf(state->dst));
	attacks &= state->occ & opp_knights(state);
	if (is_nonempty(attacks))
		return 0;

	attacks = state->rook_reach & (opp_rooks(state) | opp_queens(state));
	attacks &= state->occ;
	if (is_nonempty(attacks))
		return 0;

	attacks = state->bishop_reach;
	attacks &= opp_bishops(state) | opp_queens(state);
	attacks &= state->occ;
	if (is_nonempty(attacks))
		return 0;

	if (is_nonempty(state->dst & state->pos->attack[king + !state->side]))
		return 0;

	return 1;
}

static bool
SEE_in_check(struct SEE_state *state)
{
	int ki = bsf(state->pos->map[king + state->side]);

	uint64_t reach = sliding_map(state->occ, bishop_magics + ki);
	uint64_t bandits = opp_bishops(state) | opp_queens(state);
	bandits &= state->occ & ~state->dst;

	if (is_nonempty(bandits & reach))
		return true;

	reach = sliding_map(state->occ, rook_magics + ki);
	bandits = opp_rooks(state) | opp_queens(state);
	bandits &= state->occ & ~state->dst;

	if (is_nonempty(bandits & reach))
		return true;

	return false;
}

static int
SEE_negamax(struct SEE_state *state, int dst_value)
{
	if (state->check_pins && SEE_in_check(state)) {
		if (SEE_king(state) != 0)
			return dst_value;
		else
			return 0;
	}

	uint64_t attacker;
	int value = dst_value;
	int next_dst_value;

	if ((attacker = SEE_pawn(state)) != 0) {
		if (is_nonempty(state->dst & (RANK_8 | RANK_1))) {
			value += queen_value - pawn_value;
			next_dst_value = queen_value;
		}
		else {
			next_dst_value = pawn_value;
		}
	}
	else if ((attacker = SEE_knight(state)) != 0) {
		next_dst_value = knight_value;
	}
	else if ((attacker = SEE_bishop(state)) != 0) {
		next_dst_value = bishop_value;
	}
	else if ((attacker = SEE_rook(state)) != 0) {
		next_dst_value = rook_value;
	}
	else if ((attacker = SEE_queen(state)) != 0) {
		next_dst_value = queen_value;
	}
	else if ((attacker = SEE_king(state)) != 0) {
		return dst_value;
	}
	else {
		return 0;
	}

	state->occ &= ~attacker;
	state->side = !state->side;

	value -= SEE_negamax(state, next_dst_value);

	if (value > 0)
		return value;
	else
		return 0;
}

int
SEE(const struct position *pos, uint64_t dst, int starting_side)
{
	assert(is_empty(pos->map[starting_side] & dst));

	struct SEE_state state = {
		.pos = pos,
		.check_pins = true,
		.side = starting_side,
		.dst = dst,
		.occ = pos->occupied,
		.bishop_reach = 0,
		.rook_reach = 0
	};

	return SEE_negamax(&state, piece_value[(unsigned)pos->board[bsf(dst)]]);
}

int
SEE_move(const struct position *pos, move m)
{
	struct SEE_state state = {
		.pos = pos,
		.check_pins = true,
		.side = 1,
		.dst = mto64(m),
		.occ = (pos->occupied | mto64(m)) & ~mfrom64(m),
		.bishop_reach = 0,
		.rook_reach = 0
	};

	int value = piece_value[mcapturedp(m)];
	if (is_promotion(m))
		value += piece_value[mresultp(m)] - pawn_value;
	value -= SEE_negamax(&state, piece_value[mresultp(m)]);

	return value;
}

int
search_negative_SEE(const struct position *pos, int index)
{
	invariant(pos->board[index] != 0);

	uint64_t subject = bit64(index);
	bool side = is_nonempty(pos->map[1] & subject);
	int piece = pos->board[index];

	if (is_empty(pos->attack[!side] & subject))
		return SEE_ok;

	if (piece == king || piece == 0)
		return SEE_ok;

	if (piece != pawn
	    && is_nonempty(pos->attack[pawn + !side] & subject))
		return SEE_non_defendable;

	if (piece == queen || piece == rook) {
		if (is_nonempty(pos->attack[knight + !side] & subject))
			return SEE_non_defendable;

		if (is_nonempty(pos->attack[bishop + !side] & subject))
			return SEE_non_defendable;
	}

	if (piece == queen) {
		if (is_nonempty(pos->attack[rook + !side] & subject))
			return SEE_non_defendable;
	}

	struct SEE_state state = {
		.pos = pos,
		.check_pins = false,
		.dst = subject,
		.side = !side,
		.occ = pos->occupied,
		.bishop_reach = 0,
		.rook_reach = 0
	};

	int search_result = -SEE_negamax(&state, piece_value[piece]);

	if (search_result < 0)
		return SEE_defendable;
	else
		return SEE_ok;
}
