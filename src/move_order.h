/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_MOVE_ORDER_H
#define TALTOS_MOVE_ORDER_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "bitboard.h"
#include "chess.h"
#include "move_desc.h"

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
};

void move_order_setup(struct move_order*, const struct position*,
		    bool is_qsearch, int history_side)
	attribute(nonnull);

void move_order_pick_next(struct move_order*)
	attribute(nonnull);

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

static inline bool
mo_current_move_is_tactical(const struct move_order *mo)
{
	return mo_entry_gives_check(mo_current_entry(mo))
	    || is_capture(mo_entry_move(mo_current_entry(mo)));
}

int move_order_add_weak_hint(struct move_order*, move hint_move)
	attribute(nonnull);

int move_order_add_hint(struct move_order*, move hint_move, int16_t priority)
	attribute(nonnull);

void move_order_add_killer(struct move_order*, move killer_move)
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
