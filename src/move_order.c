
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <string.h>

#include "search.h"
#include "eval.h"
#include "move_order.h"
#include "constants.h"

static bool use_history;

struct history_value {
	uintmax_t occurence;
	uintmax_t cutoff_count; 
};

static struct history_value history[2][PIECE_ARRAY_SIZE][64];

void
move_order_setup(struct move_order *mo, const struct position *pos,
		bool is_qsearch, bool advanced, int static_value)
{
	if (is_qsearch)
		mo->count = gen_captures(pos, mo->moves);
	else
		mo->count = gen_moves(pos, mo->moves);

	mo->index = 0;
	mo->is_started = false;
	mo->is_already_sorted = false;
	mo->hint_count = 0;
	mo->advanced_order = advanced;
	mo->pos_value = static_value;
	if (advanced)
		mo->pos_threats = eval_threats(pos);
	mo->pos = pos;
}

static void
insert(struct move_order *mo, unsigned i, move m, int value, bool tactical)
{
	invariant(i < mo->count);

	while (i > 0 && mo->value[i - 1] < value) {
		mo->moves[i] = mo->moves[i - 1];
		mo->value[i] = mo->value[i - 1];
		mo->tactical[i] = mo->tactical[i - 1];
		--i;
	}

	mo->moves[i] = m;
	mo->value[i] = value;
	mo->tactical[i] = tactical;
}

static int
add_hint(struct move_order *mo, move hint_move, int value)
{
	if (hint_move == 0)
		return 0;

	for (unsigned i = 0; i < mo->count; ++i) {
		if (mo->moves[i] == hint_move) {
			if (i >= mo->hint_count) {
				mo->moves[i] = mo->moves[mo->hint_count];
				insert(mo, mo->hint_count, hint_move,
				    value, false);
				mo->hint_count++;
			}
			else if (mo->value[i] < value) {
				mo->value[i] = value;
			}
			return 0;
		}
	}

	return -1;
}

int
move_order_add_weak_hint(struct move_order *mo, move hint_move)
{
	return add_hint(mo, hint_move, -1);
}

int
move_order_add_hint(struct move_order *mo, move hint_move, int priority)
{
	assert(priority >= 0);

	if (hint_move == 0)
		return 0;

	return add_hint(mo, hint_move, INT_MAX - priority);
}

void
move_order_add_killer(struct move_order *mo, move killer_move)
{
	for (unsigned i = ARRAY_LENGTH(mo->killers) - 1; i > 0; --i)
		mo->killers[i] = mo->killers[i - 1];
	mo->killers[0] = killer_move;
}

static bool
is_killer(const struct move_order *mo, move m)
{
	for (unsigned ki = 0; ki < ARRAY_LENGTH(mo->killers); ++ki) {
		if (m == mo->killers[ki])
			return true;
	}
	return false;
}

static bool
is_knight_threat(const struct position *pos, move m)
{
	if (mresultp(m) != knight)
		return false;

	if (is_nonempty(pos->attack[opponent_pawn] & mto64(m)))
		return false;
	if (is_nonempty(pos->attack[opponent_knight] & mto64(m)))
		return false;
	if (is_nonempty(pos->attack[opponent_bishop] & mto64(m)))
		return false;

	if (is_nonempty(pos->attack[opponent_rook] & mto64(m))
	    || is_nonempty(pos->attack[opponent_queen] & mto64(m))) {
		if (is_empty(pos->attack[pawn] & mto64(m)))
			return false;
	}
	else if (is_nonempty(pos->attack[opponent_king] & mto64(m))) {
		uint64_t defend = pos->attack[pawn];
		defend |= pos->sliding_attacks[0];
		defend |= pos->attack[king];
		if (is_empty(defend & mto64(m)))
			return false;
	}

	uint64_t victims = pos->map[opponent_king];
	victims |= pos->map[opponent_rook];
	victims |= pos->map[opponent_queen];
	victims |= pos->map[opponent_bishop] & ~pos->attack[1];
	victims |= pos->map[opponent_pawn] & ~pos->attack[1];

	return popcnt(knight_pattern(mto(m)) & victims) > 1; // fork
}

static bool
is_passed_pawn_push(const struct position *pos, move m)
{
	invariant(mresultp(m) == pawn);
	invariant(!is_capture(m));

	uint64_t to = mto64(m);
	if (is_nonempty(to & (RANK_5 | RANK_6 | RANK_7))) {
		uint64_t blockers = north_of(to) | north_of(north_of(to));
		blockers &= pos->map[1];
		if (is_empty(blockers) &&
		    is_empty(to & pos->pawn_attack_reach[1]))
			return true;
	}

	return false;
}

static bool
is_pawn_fork(const struct position *pos, move m)
{
	invariant(mresultp(m) == pawn);
	invariant(!is_capture(m));

	if (is_capture(m))
		return false;

	if (is_nonempty(pos->attack[1] & mto64(m))) {
		if (is_empty(pos->attack[pawn] & mto64(m)))
			return false;
	}

	uint64_t victims = pos->map[1] & ~pos->map[opponent_pawn];
	if (popcnt(pawn_attacks_player(mto64(m)) & victims) > 1)
		return true;

	return false;
}

static int
MVVLVA(move m)
{
	return piece_value[mcapturedp(m)] - (piece_value[mresultp(m)] / 10);
}

static char quiet_move_sq_table[64] = {
	4, 4, 4, 4, 4, 4, 4, 4,
	3, 3, 3, 3, 3, 3, 3, 3,
	0, 1, 2, 3, 3, 2, 1, 0,
	0, 1, 2, 3, 3, 2, 1, 0,
	0, 1, 2, 3, 3, 2, 1, 0,
	0, 1, 2, 2, 2, 2, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static int
eval_quiet_move(struct move_order *mo, move m)
{
	int value = 0;
	uint64_t from = mfrom64(m);
	uint64_t to = mto64(m);

	if (is_nonempty(mo->pos->attack[opponent_pawn] & from)
	    && is_empty(mo->pos->attack[opponent_pawn] & to))
		value += 5; // attacked by pawn, fleeing

	if (mresultp(m) == rook) {
		uint64_t krook;

		krook = rook_pattern_table[bsf(mo->pos->map[opponent_king])];
		if (is_empty(krook & from) && is_nonempty(krook & to))
			value += 9; // threaten opponent king
	}
	else if (mresultp(m) == bishop) {
		uint64_t kbishop;

		kbishop = bishop_pattern_table[bsf(mo->pos->map[opponent_king])];
		if (is_empty(kbishop & from) && is_nonempty(kbishop & to))
			value += 9; // threaten opponent king
	}

	if (mresultp(m) == knight) {
		uint64_t victims;
		victims = mo->pos->map[opponent_bishop];
		victims |= mo->pos->map[opponent_rook];
		victims |= mo->pos->map[opponent_queen];

		if (is_nonempty(knight_pattern(mto(m)) & victims))
			++value;
	}

	value += quiet_move_sq_table[mfrom(m)];
	value += quiet_move_sq_table[mto(m)];

	if (use_history) {
		const struct history_value *h0 =
		    &(history[1][mresultp(m)][mto(m)]);
		const struct history_value *h1 =
		    &(history[0][mresultp(m)][mto(m)]);

		value +=
		    (int)((h0->cutoff_count * 30) / (h0->occurence + 1));
		value +=
		    (int)((h1->cutoff_count * 70) / (h1->occurence + 1));

	}

	return value;
}

static int
eval_move_general(struct move_order *mo, move m, bool *tactical)
{
	uint64_t to = mto64(m);

	if (mresultp(m) == pawn) {
		if (is_capture(m)) {
			*tactical = true;
			return piece_value[mcapturedp(m)] - 10;
		}

		if (is_passed_pawn_push(mo->pos, m) || is_pawn_fork(mo->pos, m)) {
			*tactical = true;
			return 3;
		}

		// en-prise
		if (is_nonempty(mo->pos->attack[1] & to)
		    && is_empty(mo->pos->attack[0] & to))
			return -1001;

		if (is_nonempty(mo->pos->attack[pawn] & to)) {
			// threat to a non-pawn piece, while being defended
			// by another pawn
			uint64_t victims = mo->pos->map[1];
			victims &= ~mo->pos->map[opponent_pawn];
			if (is_nonempty(pawn_attacks_player(to) & victims))
				return -2;
		}
		else if (is_empty(mo->pos->attack[1] & to)) {
			// threat to a non-pawn piece, while not being
			// attacked in return
			if (is_nonempty(pawn_attacks_player(to) & mo->pos->map[1]))
				return -2;
		}
		else {
			return -90 + eval_quiet_move(mo, m);
		}
	}

	if (is_knight_threat(mo->pos, m)) {
		*tactical = true;
		return 3;
	}

	if (is_capture(m)) {
		if (is_empty(mo->pos->attack[1] & to)) {
			*tactical = true;
			return piece_value[mcapturedp(m)];
		}
		if (piece_value[mcapturedp(m)] >= piece_value[mresultp(m)]) {
			*tactical = true;
			return MVVLVA(m);
		}
	}

	if (is_nonempty(to & mo->pos->attack[opponent_pawn])) {
		if (mresultp(m) != pawn)
			return -1000 - (piece_value[mresultp(m)] / 2);
	}
	else if (is_nonempty(to & mo->pos->attack[opponent_knight])
	    || is_nonempty(to & mo->pos->attack[opponent_bishop])) {
		if (mresultp(m) == rook || mresultp(m) == queen)
			return -1000 - (piece_value[mresultp(m)] / 3);
	}


	if (is_capture(m)) {
		*tactical = true;
		return MVVLVA(m);
	}

	return -100 + eval_quiet_move(mo, m);
}

static int
eval_make_move(struct move_order *mo, move m, bool *tactical)
{
	if (is_under_promotion(m)) {
		if (move_gives_check(m))
			return -1;
		else
			return -1001;
	}

	struct position child[1];

	make_move(child, mo->pos, m);
	int new_value = -eval(child);
	int new_threats = -eval_threats(child);

	int delta = new_value - mo->pos_value;
	delta += 3 * (new_threats - mo->pos_threats);
	delta -= 100;
	if (is_in_check(child))
		delta += 30;
	
	*tactical = (is_capture(m) && delta >= 0);

	return delta;
}

static int
eval_move(struct move_order *mo, move m, bool *tactical)
{
	uint64_t to = mto64(m);

	// filter out some special moves first
	switch (mtype(m)) {
		case mt_castle_kingside:
		case mt_castle_queenside:
			if (move_gives_check(m))
				return 1;
			else
				return -99;
		case mt_promotion:
			if (is_under_promotion(m)) {
				if (move_gives_check(m))
					return -100;
				else
					return -2100;
			}

			*tactical = true;

			if (is_nonempty(mo->pos->attack[1] & to))
				return 1;
			else
				return queen_value;

		case mt_en_passant:
			*tactical = true;
			return 80;

		default:
			break;
	}

	int value = eval_move_general(mo, m, tactical);

	if (move_gives_check(m)) {
		if (value < - 1000)
			value = -999;
		if (value < 0)
			value = 1;
		else
			++value;
	}

	return value;
}

static void
eval_moves(struct move_order *mo)
{
	invariant(mo->count > 0);

	if (mo->is_already_sorted)
		return;

	for (unsigned i = mo->hint_count; i < mo->count; ++i) {
		move m = mo->moves[i];
		(void) is_killer;
		if (is_killer(mo, m)) {
			insert(mo, i, m, 2, false);
		}
		else {
			bool tactical = false;
			int value;

			if (mo->advanced_order)
				value = eval_make_move(mo, m, &tactical);
			else
				value = eval_move(mo, m, &tactical);
			insert(mo, i, m, value, tactical);
		}
	}

	mo->is_already_sorted = true;
}

void
move_order_reset(struct move_order *mo)
{
	mo->is_started = false;
	mo->index = 0;
}

void
move_order_reset_to(struct move_order *mo, unsigned i)
{
	mo->is_started = true;
	mo->index = i;
}

void
move_order_pick_next(struct move_order *mo)
{
	assert(!move_order_done(mo));

	if (!mo->is_started) {
		assert(mo->index == 0);
		mo->is_started = true;
	}
	else {
		mo->index++;
	}

	if (mo->index == mo->hint_count || mo->value[mo->index] < 10000)
		eval_moves(mo);
}

void
move_order_enable_history(void)
{
	use_history = true;
}

void
move_order_disable_history(void)
{
	use_history = false;
}

void
move_order_swap_history(void)
{
	if (use_history) {
		memcpy(history[0], history[1], sizeof(*history));
		memset(history[1], 0, sizeof(history[1]));
	}
}

void
move_order_adjust_history_on_cutoff(const struct move_order *mo)
{
	if (!use_history || mo->count == 1)
		return;

	move m = mo->moves[mo->index];
	if (is_capture(m)) {
		if (piece_value[mcapturedp(m)] >= piece_value[mresultp(m)])
			return;
		if (is_empty(mo->pos->attack[1] & mto64(m)))
			return;
	}

	for (unsigned i = 0; i <= mo->index; ++i) {
		move m = mo->moves[i];
		struct history_value *h = &(history[1][mresultp(m)][mto(m)]);

		h->occurence++;
		if (i == mo->index)
			h->cutoff_count++;
	}
}
