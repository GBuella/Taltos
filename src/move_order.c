
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <string.h>

#include "search.h"
#include "eval.h"
#include "move_order.h"
#include "constants.h"

static bool use_history;

struct history_stat {
	uintmax_t occurence;
	uintmax_t cutoff_count;
};

static struct history_stat history[2][PIECE_ARRAY_SIZE][64];
static struct history_stat king_history[2][2][64][64];

void
move_order_setup(struct move_order *mo, const struct position *pos,
		bool is_qsearch, bool advanced, int static_value,
		int hside)
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
	mo->history_side = hside;
	mo->prev_from = -1;
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
	if (is_nonempty(to & (RANK_4 | RANK_5 | RANK_6 | RANK_7))) {
		uint64_t blockers = north_of(to) | north_of(north_of(to));
		blockers &= ~RANK_1;
		blockers &= pos->map[1];
		if (is_empty(blockers)
		    && is_nonempty(to & pos->half_open_files[1])
		    && is_empty(to & pos->pawn_attack_reach[1]))
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
history_value(const struct move_order *mo, move m)
{
	int value = 0;
	int s = mo->history_side;

	const struct history_stat *h0 =
	    &(history[0][mresultp(m) + s][mto(m)]);
	const struct history_stat *h1 =
	    &(history[1][mresultp(m) + s][mto(m)]);

	value += (int)((h0->cutoff_count * 15) / (h0->occurence + 100));
	value += (int)((h1->cutoff_count * 60) / (h1->occurence + 100));

	if (mresultp(m) == king) {
		//value -= 20;

		int opp_ki = bsf(mo->pos->map[opponent_king]);
		h0 = &(king_history[0][s][opp_ki][mto(m)]);
		h1 = &(king_history[1][s][opp_ki][mto(m)]);

		value += (int)((h0->cutoff_count * 15)
		    / (h0->occurence + 100));
		value += (int)((h1->cutoff_count * 60)
		    / (h1->occurence + 100));
	}

	return value;
}

static int
eval_quiet_move(struct move_order *mo, move m)
{
	int value = 0;
	uint64_t from = mfrom64(m);
	uint64_t to = mto64(m);
	int opp_ki = bsf(mo->pos->map[opponent_king]);

	if (is_nonempty(mo->pos->attack[opponent_pawn] & from)
	    && is_empty(mo->pos->attack[opponent_pawn] & to))
		value += 30; // attacked by pawn, fleeing

	if (mresultp(m) == rook || mresultp(m) == queen) {
		uint64_t krook;

		krook = rook_pattern_table[opp_ki];
		if (is_empty(krook & from) && is_nonempty(krook & to))
			value += 4; // threaten opponent king
	}
	if (mresultp(m) == bishop || mresultp(m) == queen) {
		uint64_t kbishop;

		kbishop = bishop_pattern_table[opp_ki];
		if (is_empty(kbishop & from) && is_nonempty(kbishop & to))
			value += 4; // threaten opponent king
	}

	value += quiet_move_sq_table[mfrom(m)];
	value += quiet_move_sq_table[mto(m)];

	if (use_history)
		value += history_value(mo, m);

	return value;
}

static bool
has_move_defeneder(const struct position *pos, move m,
			uint64_t rreach, uint64_t breach, uint64_t occ)
{
	uint64_t to64 = mto64(m);
	uint64_t to = mto(m);

	if (is_nonempty(pos->attack[pawn] & to64))
		return true;

	if (is_nonempty(pos->attack[king] & to64))
		return true;

	if (is_nonempty(knight_pattern(to) & pos->map[knight] & occ))
		return true;

	if (is_nonempty(rreach & pos->map[rook] & occ))
		return true;

	if (is_nonempty(breach & pos->map[bishop] & occ))
		return true;

	if (is_nonempty(breach & pos->map[queen] & occ))
		return true;

	if (is_nonempty(rreach & pos->map[queen] & occ))
		return true;

	return false;
}

static int
move_lowest_attacker(const struct position *pos, move m,
			uint64_t rreach, uint64_t breach)
{
	uint64_t to64 = mto64(m);

	if (is_nonempty(pos->attack[opponent_pawn] & to64))
		return pawn;

	if (is_nonempty(pos->attack[opponent_knight] & to64))
		return knight;

	if (is_nonempty(breach & pos->map[opponent_bishop]))
		return bishop;

	if (is_nonempty(rreach & pos->map[opponent_rook]))
		return rook;

	if (is_nonempty(rreach & pos->map[opponent_queen]))
		return queen;

	if (is_nonempty(breach & pos->map[opponent_queen]))
		return queen;

	if (is_nonempty(pos->attack[opponent_king] & to64))
		return king;

	return 0;
}

static int brevealed_attacks[PIECE_ARRAY_SIZE][4][PIECE_ARRAY_SIZE][4];
static int rrevealed_attacks[PIECE_ARRAY_SIZE][4][PIECE_ARRAY_SIZE][4];

static int
computed_revealed_attacks_map(struct move_order *mo, uint64_t occ,
		uint64_t reach,
		const uint64_t pattern_table[static 64],
		int revealed_attacks[PIECE_ARRAY_SIZE][4][PIECE_ARRAY_SIZE][4])
{
	int value = 0;
	reach &= occ;

	while (is_nonempty(reach)) {
		uint64_t bit0 = lsb(reach);
		int index0 = bsf(reach);
		reach = reset_lsb(reach);
		uint64_t bit1 = pattern_table[index0] & reach;

		if (is_empty(bit1))
			continue;

		reach ^= bit1;
		int index1 = bsf(bit1);

		int piece0 = mo->pos->board[index0];
		int piece1 = mo->pos->board[index1];
		piece0 += is_empty(mo->pos->map[0] & bit0);
		piece1 += is_empty(mo->pos->map[0] & bit1);
		int attack0 = is_nonempty(mo->pos->attack[0] & bit0);
		attack0 += 2 * is_nonempty(mo->pos->attack[1] & bit0);
		int attack1 = is_nonempty(mo->pos->attack[0] & bit1);
		attack1 += 2 * is_nonempty(mo->pos->attack[1] & bit1);
		value += revealed_attacks[piece0][attack0][piece1][attack1];
	}

	return value;
}

static void
compute_revealed_attacks(struct move_order *mo, move m)
{
	int from = mfrom(m);
	int piece = mresultp(m);

	assert(!is_promotion(m));
	assert(!(mresultp(m) == pawn && is_capture(m)));

	if (piece == queen) {
		mo->revealed_attack = 0;
		return;
	}

	if (is_empty(mfrom64(m) & mo->pos->sliding_attacks[0])
	    && is_empty(mfrom64(m) & mo->pos->sliding_attacks[1])) {
		mo->revealed_attack = 0;
		return;
	}

	if (mo->prev_from == from && mresultp(m) != king && mresultp(m) != pawn)
		return;

	uint64_t new_occ = mo->pos->occupied & ~mfrom64(m);
	if (piece == pawn || piece == king)
		new_occ |= mto64(m);

	mo->revealed_attack = 0;

	if (piece != rook) {
		uint64_t reach = sliding_map(new_occ, rook_magics + from);
		mo->revealed_attack += computed_revealed_attacks_map(mo,
		    new_occ, reach,
		    rook_pattern_table, rrevealed_attacks);
	}

	if (piece != bishop) {
		uint64_t reach = sliding_map(new_occ, bishop_magics + from);
		mo->revealed_attack += computed_revealed_attacks_map(mo,
		    new_occ, reach,
		    bishop_pattern_table, brevealed_attacks);
	}

	mo->prev_from = from;
}

static int
eval_move_general(struct move_order *mo, move m, bool *tactical)
{
	uint64_t to64 = mto64(m);
	int to = mto(m);
	int piece = mresultp(m);
	uint64_t opp_en_prise =
	    mo->pos->map[1] & ~mo->pos->attack[1] & ~mo->pos->attack[0];
	uint64_t en_prise =
	    mo->pos->map[0] & mo->pos->attack[1] & ~mo->pos->attack[0];

	if (piece == pawn) {
		if (is_capture(m)) {
			*tactical = true;
			return piece_value[mcapturedp(m)] - 10;
		}

		if (is_passed_pawn_push(mo->pos, m)
		    || is_pawn_fork(mo->pos, m)) {
			*tactical = true;
			return 22;
		}

		// en-prise
		if (is_nonempty(mo->pos->attack[1] & to64)
		    && is_empty(mo->pos->attack[0] & to64))
			return -1001;

		uint64_t victims = mo->pos->map[1];
		victims &= ~mo->pos->map[opponent_pawn];
		victims &= pawn_attacks_player(to64);

		if (is_nonempty(mo->pos->attack[pawn] & to64)) {
			// threat to a non-pawn piece, while being defended
			// by another pawn
			if (is_nonempty(victims & mo->pos->map[opponent_queen]))
				return 18;
			else if (is_nonempty(victims & mo->pos->map[opponent_rook]))
				return 17;
			else if (is_nonempty(victims))
				return 12;
		}
		else if (is_empty(mo->pos->attack[1] & to64)) {
			// threat to a non-pawn piece, while not being
			// attacked in return
			if (is_nonempty(victims & mo->pos->map[opponent_rook]))
				return 17;
			else if (is_nonempty(victims))
				return 12;
		}

		int base = -240;
		if (is_nonempty(pawn_attacks_player(to64) & en_prise))
			base = -210;
		else if (to == sq_b3
		    && is_nonempty(mo->pos->map[pawn] & SQ_C2)
		    && is_nonempty(mo->pos->map[opponent_bishop] & SQ_A2))
			base = -220;
		else if (to == sq_g3
		    && is_nonempty(mo->pos->map[pawn] & SQ_F2)
		    && is_nonempty(mo->pos->map[opponent_bishop] & SQ_H2))
			base = -220;

		return base + eval_quiet_move(mo, m);
	}

	if (is_knight_threat(mo->pos, m)) {
		*tactical = true;
		return 21;
	}

	if (is_capture(m)) {
		if (is_empty(mo->pos->attack[1] & to64)) {
			*tactical = true;
			return piece_value[mcapturedp(m)];
		}
		if (piece_value[mcapturedp(m)] >= piece_value[piece]) {
			*tactical = true;
			return MVVLVA(m);
		}
		if (mresultp(m) == rook) {
			if (mcapturedp(m) == bishop || mcapturedp(m) == knight) {
				*tactical = true;
				return MVVLVA(m);
			}
		}
		if (mresultp(m) == bishop || mresultp(m) == knight) {
			if (is_nonempty(to64 & mo->pos->attack[opponent_king])) {
				*tactical = true;
				return MVVLVA(m);
			}
		}
	}

	if (is_nonempty(to64 & mo->pos->attack[opponent_pawn]))
		return -1400 + MVVLVA(m);

	uint64_t occ = (mo->pos->occupied ^ mfrom64(m)) | to64;
	uint64_t breach = sliding_map(occ, bishop_magics + to);
	uint64_t rreach = sliding_map(occ, rook_magics + to);

	int lowest_opp_defender =
	    move_lowest_attacker(mo->pos, m, rreach, breach);

	if (lowest_opp_defender != 0) {
		if (has_move_defeneder(mo->pos, m, rreach, breach, occ)) {
			int att_value = 1000;
			if (lowest_opp_defender != king)
				att_value = piece_value[lowest_opp_defender];
			if (att_value < piece_value[piece])
				return -900 - (piece_value[piece] / 4);
		}
		else {
			return -1400 + MVVLVA(m);
		}
	}

	if (is_capture(m)) {
		*tactical = true;
		return MVVLVA(m);
	}

	int base = -240;

	if (piece == knight) {
		uint64_t reach = knight_pattern(to);
		if (is_nonempty(reach &mo->pos->map[opponent_queen]))
			return 19;
		else if (is_nonempty(reach & mo->pos->map[opponent_rook]))
			return 16;
		else if (is_nonempty(reach & opp_en_prise & ~mo->pos->map[opponent_knight]))
			return 15;
		else if (is_nonempty(reach & en_prise))
			base = -200;
	}
	else if (piece == bishop) {
		breach &= ~mo->pos->map[opponent_bishop];
		if (has_move_defeneder(mo->pos, m, rreach, breach, occ)
		    && is_nonempty(breach & mo->pos->map[opponent_queen]))
			return 19;
			//	base = -100;
		else if (is_nonempty(breach & mo->pos->map[opponent_rook]))
			return 16;
		else if (is_nonempty(breach & opp_en_prise))
			return 15;
			//base = -110;
		else if (is_nonempty(breach & en_prise))
			base = -200;
			//base = -120;
	}
	else if (piece == rook) {
		rreach &= ~mo->pos->map[opponent_rook];
		if (has_move_defeneder(mo->pos, m, rreach, breach, occ)
		    && is_nonempty(rreach & mo->pos->map[opponent_queen]))
			return 19;
			//base = -100;
		else if (is_nonempty(rreach & opp_en_prise))
			return 15;
			//base = -110;
		else if (is_nonempty(rreach & en_prise))
			base = -200;
	}
	else if (piece == queen) {
		opp_en_prise &= ~mo->pos->map[opponent_queen];
		if (is_nonempty(breach & opp_en_prise))
			return 14;
			//base = -110;
		else if (is_nonempty(rreach & opp_en_prise))
			return 14;
			//base = -110;
		else if (is_nonempty(rreach & en_prise))
			base = -200;
		else if (is_nonempty(breach & en_prise))
			base = -200;
	}
	else if (piece == king) {

		/*
		uint64_t new_reach = king_moves_table[to];
		if (is_nonempty(new_reach & opp_en_prise))
			base = -120;
		else if (is_nonempty(new_reach & en_prise))
			base = -121;
			*/

		uint64_t new_reach = king_moves_table[to];
		if (is_nonempty(new_reach & opp_en_prise))
			return 10;
			//base = -121;
		else if (is_nonempty(new_reach & en_prise))
			base = -220;
	}

	return base + eval_quiet_move(mo, m);
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

static bool
move_saves_queen(struct move_order *mo, move m)
{
	if (is_empty(mo->pos->map[queen] & mo->pos->attack[1]))
		return false;

	if (mresultp(m) == queen
	    && is_nonempty(mo->pos->attack[1] & mfrom64(m))) {
		if (is_empty(mo->pos->attack[1] & mto64(m)))
			return true;
	}

	return false;
}

static bool
move_saves_rook(struct move_order *mo, move m)
{
	if (is_empty(mo->pos->map[rook] & mo->pos->attack[1]))
		return false;

	if (mresultp(m) == rook
	    && is_nonempty(mo->pos->attack[1] & mfrom64(m))) {
		if (is_empty(mo->pos->attack[1] & mto64(m)))
			return true;
	}

	return false;
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
				return 2;
			else if (is_nonempty(mo->pos->map[king]
				    & mo->pos->half_open_files[0]))
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

	if (mresultp(m) == pawn && is_capture(m)) {
		*tactical = true;
		return piece_value[mcapturedp(m)] - 10;
	}

	int value = eval_move_general(mo, m, tactical);

	if (move_gives_check(m)) {
		if (value < - 1000 && is_capture(m))
			value = -997;
		else if (value < - 1000 && !is_capture(m))
			++value;
		else if (value < 0)
			value = 1;
		else
			++value;
	}

	compute_revealed_attacks(mo, m);
	if (mo->revealed_attack > 5 && value <= 0 && value > - 1000)
		value = mo->revealed_attack;
	else if (mo->revealed_attack > 17 && value < - 1000
	    && mresultp(m) == pawn)
		value = 4;
	else
		value += mo->revealed_attack;

	if (value < 1000 && mresultp(m) == knight
	    && is_nonempty(mo->pos->sliding_attacks[0])) {
		int opp_ki = bsf(mo->pos->map[opponent_king]);
		if (is_nonempty(to & king_reach_2_table[opp_ki]))
			value = -998;
		else if (is_nonempty(to & king_reach_4_table[opp_ki]))
			value = -999;
	}

	/*
	if (move_saves_queen(mo, m))
		value += queen_value / 2;
	else if (move_saves_rook(mo, m))
		value += rook_value / 2;
	*/
	if (move_saves_queen(mo, m))
		value += 100;
	else if (move_saves_rook(mo, m))
		value += 50;

	(void) move_saves_rook;

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
		if (is_killer(mo, m)) {
			insert(mo, i, m, 20, false);
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
		memcpy(king_history[0], king_history[1], sizeof(*king_history));
		memset(king_history[1], 0, sizeof(king_history[1]));
	}
}

void
move_order_adjust_history_on_cutoff(const struct move_order *mo)
{
	if (!use_history)
		return;

	if (mo->count == 1)
		return;

	move best = mo->moves[mo->index];
	int side = mo->history_side;
	if (is_capture(best)) {
		if (piece_value[mcapturedp(best)] >=
		    piece_value[mresultp(best)])
			return;
		if (is_empty(mo->pos->attack[1] & mto64(best)))
			return;
	}

	for (unsigned i = 0; i <= mo->index; ++i) {
		move m = mo->moves[i];
		struct history_stat *h =
		    &(history[1][mresultp(m) + side][mto(m)]);

		h->occurence++;
		if (i == mo->index)
			h->cutoff_count++;

		if (mresultp(m) == king && mresultp(best) == king) {
			int opp_ki = bsf(mo->pos->map[opponent_king]);
			h = &(king_history[1][side][opp_ki][mto(m)]);

			h->occurence++;
			if (i == mo->index)
				h->cutoff_count++;
		}
	}
}

void
init_move_order(void)
{
	static const int at0 = 1;
	static const int at1 = 2;
	static const int at01 = 3;

	for (int p = 1; p < PIECE_ARRAY_SIZE; p += 2) {
		if (p == pawn)
			continue;

		brevealed_attacks[bishop][0][p][0] = 10;
		brevealed_attacks[bishop][at0][p][0] = 10;
		brevealed_attacks[bishop][at01][p][0] = 9;
		brevealed_attacks[queen][0][p][0] = 10;
		brevealed_attacks[queen][at0][p][0] = 10;
		brevealed_attacks[queen][at01][p][0] = 9;

		rrevealed_attacks[rook][0][p][0] = 10;
		rrevealed_attacks[rook][at0][p][0] = 10;
		rrevealed_attacks[rook][at01][p][0] = 9;
		rrevealed_attacks[queen][0][p][0] = 10;
		rrevealed_attacks[queen][at0][p][0] = 10;
		rrevealed_attacks[queen][at01][p][0] = 9;
	}

	brevealed_attacks[bishop][at0][opponent_queen][at1] = 19;
	rrevealed_attacks[rook][at0][opponent_queen][at1] = 19;

	for (int at0 = 0; at0 < 4; ++at0) {
		for (int at1 = 0; at1 < 4; ++at1) {
			rrevealed_attacks[rook][at0][rook][at1] = 5;
			brevealed_attacks[bishop][at0][bishop][at1] = 5;
			brevealed_attacks[bishop][at0][queen][at1] = 5;
		}
	}

	for (int p0 = 2; p0 < PIECE_ARRAY_SIZE; ++p0) {
		for (int at0 = 0; at0 < 4; ++at0) {
			for (int at1 = 0; at1 < 4; ++at1) {
				brevealed_attacks[p0][at0][bishop][at1] =
				    brevealed_attacks[bishop][at1][p0][at0];
				/*
				brevealed_attacks[p0][at0][bishop + 1][at1] =
				    brevealed_attacks[bishop + 1][at1][p0][at0];
				    */
				brevealed_attacks[p0][at0][queen][at1] =
				    brevealed_attacks[queen][at1][p0][at0];
				/*
				brevealed_attacks[p0][at0][queen + 1][at1] =
				    brevealed_attacks[queen + 1][at1][p0][at0];
				    */

				rrevealed_attacks[p0][at0][rook][at1] =
				    rrevealed_attacks[rook][at1][p0][at0];
				/*
				rrevealed_attacks[p0][at0][rook + 1][at1] =
				    rrevealed_attacks[rook + 1][at1][p0][at0];
				    */
				rrevealed_attacks[p0][at0][queen][at1] =
				    rrevealed_attacks[queen][at1][p0][at0];
				/*
				rrevealed_attacks[p0][at0][queen + 1][at1] =
				    rrevealed_attacks[queen + 1][at1][p0][at0];
				    */
			}
		}
	}
}
