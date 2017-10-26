/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#include <assert.h>
#include <string.h>

#include "search.h"
#include "eval.h"
#include "move_order.h"
#include "constants.h"

static int64_t
create_entry(move move, int16_t value, bool is_check)
{
	int64_t entry = move;
	entry |= value * (INT64_C(1) << (64 - mo_entry_value_bits));
	if (is_check)
		entry |= bit64(mo_entry_check_flag_bit);

	return entry;
}

static int64_t
create_hint_entry(move move, int16_t value, bool is_check)
{
	int64_t entry = create_entry(move, value, is_check);
	entry |= bit64(mo_entry_hint_flag_bit);

	return entry;
}

static int64_t
reset_entry_value(int64_t entry, int16_t value)
{
	entry &= (bit64(64 - mo_entry_value_bits) - 1);
	entry |= value * (INT64_C(1) << (64 - mo_entry_value_bits));
	return entry;
}

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
insert_at(struct move_order *mo, unsigned i, int64_t entry)
{
	invariant(i >= mo->picked_count);
	invariant(i <= mo->entry_count);

	while (i > mo->picked_count && mo->entries[i - 1] < entry) {
		mo->entries[i] = mo->entries[i - 1];
		--i;
	}
	mo->entries[i] = entry;
}

static void
insert(struct move_order *mo, int64_t entry)
{
	insert_at(mo, mo->entry_count, entry);
	mo->entry_count++;
}

static void
remove_raw_move(struct move_order *mo, unsigned i)
{
	invariant(mo->raw_move_count > 0);
	invariant(i < mo->raw_move_count);

	mo->moves[i] = mo->moves[mo->raw_move_count - 1];
	mo->raw_move_count--;
}

static int
add_hint(struct move_order *mo, move hint_move, int16_t value)
{
	if (hint_move == 0)
		return 0;

	for (unsigned i = mo->picked_count; i < mo->entry_count; ++i) {
		int64_t entry = mo->entries[i];
		if (mo_entry_move(entry) == hint_move) {
			if (mo_entry_value(entry) < value) {
				entry = reset_entry_value(entry, value);
				insert_at(mo, i, entry);
			}
			return 0;
		}
	}

	for (unsigned i = 0; i < mo->raw_move_count; ++i) {
		if (mo->moves[i] == hint_move) {
			remove_raw_move(mo, i);
			insert(mo, create_hint_entry(hint_move, value, false));
			return 0;
		}
	}

	return -1;
}

int
move_order_add_weak_hint(struct move_order *mo, move hint_move)
{
	return add_hint(mo, hint_move, 3000);
}

int
move_order_add_hint(struct move_order *mo, move hint_move, int16_t priority)
{
	assert(priority >= 0);

	if (hint_move == 0)
		return 0;

	return add_hint(mo, hint_move, INT16_MAX - priority);
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

/*
 * Strong captures, that remove strong pieces from the board, thus
 * resulting in smaller subtrees than other moves.
 */
static bool
is_strong_capture(const struct position *pos, move m)
{
	if (!is_capture(m))
		return false;

	if (mcapturedp(m) == queen)
		return true;

	if (mtype(m) == mt_en_passant)
		return true;

	if (mtype(m) == mt_promotion)
		return mresultp(m) == queen;

	if (ind_rank(mto(m)) == rank_1) {
		/*
		 * Such a capture would introduce a new queen on the
		 * next move (when the pawns recaptures), and enlarge
		 * the searchtree, instead of shrinking it.
		 */
		if (is_nonempty(mto64(m) & pos->attack[opponent_pawn]))
			return false;
	}

	if (mcapturedp(m) == rook)
		return true;

	if (is_empty(mto64(m) & pos->attack[1]))
		return true;

	if (piece_value[mcapturedp(m)] >= piece_value[mresultp(m)])
		return true;

	return false;
}

static void
add_strong_capture_entries(struct move_order *mo)
{
	for (unsigned i = 0; i < mo->raw_move_count; ) {
		move m = mo->moves[i];
		if (is_strong_capture(mo->pos, m)) {
			remove_raw_move(mo, i);
			int value = 1000 + piece_value[mcapturedp(m)];
			if (is_nonempty(mto64(m) & mo->pos->attack[1]))
				value -= piece_value[mresultp(m)] / 20;
			insert(mo, create_entry(m, value, false));
		}
		else {
			++i;
		}
	}
	mo->strong_capture_entries_added = true;
}

static int16_t
move_history_value(const struct move_order *mo, move m)
{
	const struct history_value *h0 =
	    &(history[0][mresultp(m) + mo->history_side][mto(m)]);
	const struct history_value *h1 =
	    &(history[1][mresultp(m) + mo->history_side][mto(m)]);

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
		move m = mo->moves[i];

		describe_move(&mo->desc, mo->pos, m);
		bool check = mo->desc.direct_check;
		check |= mo->desc.discovered_check;

		int16_t value = base;
		value += mo->desc.value;
		if (use_history)
			value += move_history_value(mo, m);
		if (value > -150 && value < killer_value && is_killer(mo, m))
			value = killer_value;
		insert(mo, create_entry(m, value, check));
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

	move best = mo_current_move(mo);
	int side = mo->history_side;
	if (is_capture(best)) {
		if (piece_value[mcapturedp(best)] >=
		    piece_value[mresultp(best)])
			return;
		if (is_empty(mo->pos->attack[1] & mto64(best)))
			return;
	}

	for (unsigned i = 0; i < mo->picked_count; ++i) {
		move m = mo_entry_move(mo->entries[i]);
		struct history_value *h =
		    &(history[1][mresultp(m) + side][mto(m)]);

		h->occurence++;
		if (i + 1 == mo->picked_count)
			h->cutoff_count++;
	}
}
