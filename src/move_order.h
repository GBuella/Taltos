
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_MOVE_ORDER_H
#define TALTOS_MOVE_ORDER_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "chess.h"

struct move_order {
	move moves[MOVE_ARRAY_LENGTH];
	int value[MOVE_ARRAY_LENGTH];
	bool tactical[MOVE_ARRAY_LENGTH];
	unsigned index;
	unsigned count;
	move killers[2];
	bool is_started;
	bool is_already_sorted;
	unsigned hint_count;
	bool advanced_order;
	int pos_value;
	int pos_threats;
	const struct position *pos;
};

void move_order_setup(struct move_order*, const struct position*,
		    bool is_qsearch, bool advanced, int static_value)
	attribute(nonnull);

void move_order_pick_next(struct move_order*)
	attribute(nonnull);

static inline move
mo_current_move(const struct move_order *mo)
{
	return mo->moves[mo->index];
}

static inline move
mo_is_on_first_move(const struct move_order *mo)
{
	return mo->index == 0;
}

static inline move
mo_current_move_value(const struct move_order *mo)
{
	return mo->value[mo->index];
}

static inline bool
mo_current_move_is_tactical(const struct move_order *mo)
{
	return mo->tactical[mo->index];
}


int move_order_add_weak_hint(struct move_order*, move hint_move)
	attribute(nonnull);

int move_order_add_hint(struct move_order*, move hint_move, int priority)
	attribute(nonnull);

void move_order_add_killer(struct move_order*, move killer_move)
	attribute(nonnull);

static inline unsigned
move_order_remaining(const struct move_order *mo)
{
	return mo->count - mo->index;
}

static inline bool
move_order_done(const struct move_order *mo)
{
	return mo->is_started && (mo->index + 1 >= mo->count);
}

void move_order_reset(struct move_order*)
	attribute(nonnull);

void move_order_reset_to(struct move_order*, unsigned i)
	attribute(nonnull);

void move_order_adjust_history_on_cutoff(const struct move_order*)
	attribute(nonnull);

void move_order_enable_history(void);
void move_order_disable_history(void);
void move_order_swap_history(void);

#endif
