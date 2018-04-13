/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2018, Gabor Buella
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
		bool is_qsearch, int hside)
{
	if (is_qsearch)
		mo->count = gen_captures(pos, mo->moves);
	else
		mo->count = gen_moves(pos, mo->moves);

	for (unsigned i = 0; i < mo->count; ++i)
		mo->move_is_scored[i] = false;
	mo->raw_move_count = mo->count;
	mo->entry_count = 0;
	mo->picked_count = 0;
	mo->is_started = false;
	mo->is_already_sorted = false;
	mo->hint_count = 0;
	mo->pos = pos;
	mo->history_side = hside;
	mo->strong_capture_entries_added = false;
	mo->LMR_subject_index = -1;
	move_desc_setup(&mo->desc);
}

static void
insert_at(struct move_order *mo, unsigned i, struct mo_entry entry)
{
	invariant(i >= mo->picked_count);
	invariant(i <= mo->entry_count);

	while (i > mo->picked_count && mo->entries[i - 1].score < entry.score) {
		mo->entries[i] = mo->entries[i - 1];
		--i;
	}
	mo->entries[i] = entry;
}

static void
insert(struct move_order *mo, struct mo_entry entry)
{
	insert_at(mo, mo->entry_count, entry);
	mo->entry_count++;
}

static int
add_hint(struct move_order *mo, struct move hint_move, int16_t score)
{
	if (is_null_move(hint_move))
		return 0;

	for (unsigned i = mo->picked_count; i < mo->entry_count; ++i) {
		struct mo_entry entry = mo->entries[i];
		if (move_eq(entry.move, hint_move)) {
			if (entry.score < score) {
				entry.score = score;
				insert_at(mo, i, entry);
			}
			return 0;
		}
	}

	for (unsigned i = 0; i < mo->raw_move_count; ++i) {
		if (move_eq(mo->moves[i], hint_move)) {
			assert(!mo->move_is_scored[i]);
			insert(mo, (struct mo_entry) {
			       .move = hint_move,
			       .mg_index = i,
			       .gives_check = false,
			       .is_hint = true,
			       .score = score });
			mo->move_is_scored[i] = true;
			return 0;
		}
	}

	return -1;
}

int
move_order_add_weak_hint(struct move_order *mo, struct move hint_move)
{
	return add_hint(mo, hint_move, 3000);
}

int
move_order_add_hint(struct move_order *mo, struct move hint_move,
		    int16_t priority)
{
	assert(priority >= 0);

	if (is_null_move(hint_move))
		return 0;

	return add_hint(mo, hint_move, INT16_MAX - priority);
}

int
move_order_add_hint_by_mg_index(struct move_order *mo, uint8_t mg_index,
				int16_t priority)
{
	assert(priority >= 0);

	if (mg_index >= mo->count)
		return 1;

	if (!mo->move_is_scored[mg_index]) {
		insert(mo, (struct mo_entry) {
		       .move = mo->moves[mg_index],
		       .mg_index = mg_index,
		       .gives_check = false,
		       .is_hint = true,
		       .score = INT16_MAX - priority });
		mo->move_is_scored[mg_index] = true;
		return 0;
	}
	else {
		return add_hint(mo, mo->moves[mg_index], INT16_MAX - priority);
	}
}

void
move_order_add_killer(struct move_order *mo, struct move killer_move)
{
	for (unsigned i = ARRAY_LENGTH(mo->killers) - 1; i > 0; --i)
		mo->killers[i] = mo->killers[i - 1];
	mo->killers[0] = killer_move;
}

static bool
is_killer(const struct move_order *mo, struct move m)
{
	for (unsigned ki = 0; ki < ARRAY_LENGTH(mo->killers); ++ki) {
		if (move_eq(m, mo->killers[ki]))
			return true;
	}
	return false;
}

/*
 * Strong captures, that remove strong pieces from the board, thus
 * resulting in smaller subtrees than other moves.
 */
static bool
is_strong_capture(const struct position *pos, struct move m)
{
	if (!is_capture(m))
		return false;

	if (m.captured == queen)
		return true;

	if (m.type == mt_en_passant)
		return true;

	if (m.type == mt_promotion)
		return m.result == queen;

	if (ind_rank(m.to) == rank_1) {
		/*
		 * Such a capture would introduce a new queen on the
		 * next move (when the pawns recaptures), and enlarge
		 * the searchtree, instead of shrinking it.
		 */
		if (is_nonempty(mto64(m) & pos->attack[pos->opponent | pawn]))
			return false;
	}

	if (m.captured == rook)
		return true;

	if (is_empty(mto64(m) & pos->attack[pos->opponent]))
		return true;

	if (piece_value[m.captured] >= piece_value[m.result])
		return true;

	return false;
}

static void
add_strong_capture_entries(struct move_order *mo)
{
	for (unsigned i = 0; i < mo->raw_move_count; ++i) {
		if (mo->move_is_scored[i])
			continue;

		struct move m = mo->moves[i];
		if (is_strong_capture(mo->pos, m)) {
			int score = 1000 + piece_value[m.captured];
			uint64_t attacked = mo->pos->attack[mo->pos->opponent];
			if (is_nonempty(mto64(m) & attacked))
				score -= piece_value[m.result] / 20;
			mo->move_is_scored[i] = true;
			insert(mo, (struct mo_entry) {
			       .move = m,
			       .mg_index = i,
			       .gives_check = false,
			       .is_hint = false,
			       .score = score
			       });
		}
	}
	mo->strong_capture_entries_added = true;
}

static int16_t
move_history_value(const struct move_order *mo, struct move m)
{
	const struct history_value *h0 =
	    &(history[0][m.result + mo->history_side][m.to]);
	const struct history_value *h1 =
	    &(history[1][m.result + mo->history_side][m.to]);

	int16_t value = 0;

	value += (int16_t)((h0->cutoff_count * 15) / (h0->occurence + 20));
	value += (int16_t)((h1->cutoff_count * 60) / (h1->occurence + 100));

	return value;
}

static void
add_all_entries(struct move_order *mo)
{
	static const int16_t base = -100;

	for (unsigned i = 0; i < mo->raw_move_count; ++i) {
		if (mo->move_is_scored[i])
			continue;

		struct move m = mo->moves[i];

		describe_move(&mo->desc, mo->pos, m);
		bool check = mo->desc.direct_check;
		check |= mo->desc.discovered_check;

		int16_t score = base;
		score += mo->desc.value;
		if (use_history)
			score += move_history_value(mo, m);
		if (score > -150 && score < killer_value && is_killer(mo, m))
			score = killer_value;
		insert(mo, (struct mo_entry) {
		       .move = m,
		       .score = score,
		       .gives_check = check,
		       .is_hint = false,
		       .mg_index = i });
	}
	mo->raw_move_count = 0;
}

void
move_order_pick_next(struct move_order *mo)
{
	invariant(!move_order_done(mo));

	while (mo->picked_count == mo->entry_count) {
		if (!mo->strong_capture_entries_added)
			add_strong_capture_entries(mo);
		else
			add_all_entries(mo);
	}

	invariant(mo->picked_count < mo->entry_count);
	mo->picked_count++;

	if (mo->picked_count > 1 && mo_current_move_value(mo) < 1000)
		mo->LMR_subject_index++;
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
	if (!use_history)
		return;

	if (mo->count == 1)
		return;

	struct move best = mo_current_move(mo);
	int side = mo->history_side;
	if (is_capture(best)) {
		if (piece_value[best.captured] >= piece_value[best.result])
			return;
		if (is_empty(mo->pos->attack[mo->pos->opponent] & mto64(best)))
			return;
	}

	for (unsigned i = 0; i < mo->picked_count; ++i) {
		struct move m = mo->entries[i].move;
		struct history_value *h =
		    &(history[1][m.result + side][m.to]);

		h->occurence++;
		if (i + 1 == mo->picked_count)
			h->cutoff_count++;
	}
}
