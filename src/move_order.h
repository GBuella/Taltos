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

#ifndef TALTOS_MOVE_ORDER_H
#define TALTOS_MOVE_ORDER_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "bitboard.h"
#include "chess.h"
#include "move_desc.h"

enum {
	killer_value = 70
};

struct mo_entry {
	struct move move;
	uint8_t mg_index;
	bool gives_check;
	bool is_hint;
	int16_t score;
};

struct move_order {
	struct move moves[MOVE_ARRAY_LENGTH];
	bool move_is_scored[MOVE_ARRAY_LENGTH];
	unsigned raw_move_count;
	struct mo_entry entries[MOVE_ARRAY_LENGTH];
	unsigned entry_count;

	struct move_desc desc;
	unsigned count;
	unsigned picked_count;
	struct move killers[2];
	bool is_started;
	bool is_already_sorted;
	unsigned hint_count;
	const struct position *pos;
	int history_side;

	bool strong_capture_entries_added;

	int LMR_subject_index;
};

void move_order_setup(struct move_order*, const struct position*,
		    bool is_qsearch, int history_side)
	attribute(nonnull);

void move_order_pick_next(struct move_order*)
	attribute(nonnull);

static inline struct mo_entry
mo_current_entry(const struct move_order *mo)
{
	invariant(mo->picked_count > 0);
	return mo->entries[mo->picked_count - 1];
}

static inline struct move
mo_current_move(const struct move_order *mo)
{
	struct mo_entry entry = mo_current_entry(mo);
	return entry.move;
}

static inline uint8_t
mo_current_move_index(const struct move_order *mo)
{
	struct mo_entry entry = mo_current_entry(mo);
	return entry.mg_index;
}

static inline int16_t
mo_current_move_value(const struct move_order *mo)
{
	struct mo_entry entry = mo_current_entry(mo);
	return entry.score;
}

int move_order_add_weak_hint(struct move_order*, struct move hint_move)
	attribute(nonnull);

int move_order_add_hint(struct move_order*, struct move hint_move,
			int16_t priority)
	attribute(nonnull);

int move_order_add_hint_by_mg_index(struct move_order*, uint8_t mg_index,
			int16_t priority)
	attribute(nonnull);

void move_order_add_killer(struct move_order*, struct move killer_move)
	attribute(nonnull);

static inline unsigned
move_order_remaining(const struct move_order *mo)
{
	return mo->count - mo->picked_count;
}

static inline bool
move_order_done(const struct move_order *mo)
{
	return mo->picked_count == mo->count;
}

void move_order_adjust_history_on_cutoff(const struct move_order*)
	attribute(nonnull);

void move_order_enable_history(void);
void move_order_disable_history(void);
void move_order_swap_history(void);

#endif
