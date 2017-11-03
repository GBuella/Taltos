/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#ifndef TALTOS_MOVE_ORDER_H
#define TALTOS_MOVE_ORDER_H

#include <climits>
#include <cstdint>

#include "bitboard.h"
#include "chess.h"
#include "move_desc.h"

namespace taltos
{

enum {
	mo_entry_move_bits = 32,
	mo_entry_value_bits = 16,
	mo_entry_check_flag_bit = 33,
	mo_entry_hint_flag_bit = 34
};

static inline move
mo_entry_move(int64_t entry)
{
	return (move)(entry & (bit64(mo_entry_move_bits) - 1));
}

static inline int16_t
mo_entry_value(int64_t entry)
{
	return entry / (INT64_C(1) << (64 - mo_entry_value_bits));
}

static inline bool
mo_entry_gives_check(int64_t entry)
{
	return (entry & bit64(mo_entry_check_flag_bit)) != 0;
}

static inline bool
mo_entry_is_hint(int64_t entry)
{
	return (entry & bit64(mo_entry_hint_flag_bit)) != 0;
}

enum {
	killer_value = 70
};

struct move_order {
	move moves[MOVE_ARRAY_LENGTH];
	unsigned raw_move_count;
	int64_t entries[MOVE_ARRAY_LENGTH];
	unsigned entry_count;

	struct move_desc desc;
	unsigned count;
	unsigned picked_count;
	move killers[2];
	bool is_started;
	bool is_already_sorted;
	unsigned hint_count;
	const struct position *pos;
	int history_side;

	bool strong_capture_entries_added;

	int LMR_subject_index;
};

void move_order_setup(struct move_order*, const struct position*,
		    bool is_qsearch, int history_side);

void move_order_pick_next(struct move_order*);

static inline int64_t
mo_current_entry(const struct move_order *mo)
{
	invariant(mo->picked_count > 0);
	return mo->entries[mo->picked_count - 1];
}

static inline move
mo_current_move(const struct move_order *mo)
{
	return mo_entry_move(mo_current_entry(mo));
}

static inline int16_t
mo_current_move_value(const struct move_order *mo)
{
	return mo_entry_value(mo_current_entry(mo));
}

int move_order_add_weak_hint(struct move_order*, move hint_move);

int move_order_add_hint(struct move_order*, move hint_move, int16_t priority);

void move_order_add_killer(struct move_order*, move killer_move);

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

void move_order_adjust_history_on_cutoff(const struct move_order*);

void move_order_enable_history(void);
void move_order_disable_history(void);
void move_order_swap_history(void);

}

#endif
