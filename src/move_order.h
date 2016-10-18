
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_MOVE_ORDER_H
#define TALTOS_MOVE_ORDER_H

#include "chess.h"

struct move_fsm {
	move moves[MOVE_ARRAY_LENGTH];
	int values[MOVE_ARRAY_LENGTH];
	bool has_hashmove;
	unsigned killerm_end;
	unsigned tacticals_end;
	unsigned very_late_moves_begin;
	unsigned count;
	unsigned index;
	unsigned hash_move_count;
	move hash_moves[2];
	move killers[2];
	bool is_in_late_move_phase;
	bool already_ordered;
};



void move_fsm_setup(struct move_fsm*, const struct position*, bool is_qsearch)
	attribute(nonnull);

move select_next_move(const struct position*, struct move_fsm*)
	attribute(nonnull);

static inline int
move_fsm_add_hash_move(struct move_fsm *fsm, move hash_move)
{
	if (hash_move == 0)
		return 0;

	for (unsigned i = 0; i < fsm->count; ++i) {
		if (fsm->moves[i] == hash_move) {
			fsm->moves[i] = fsm->moves[0];
			fsm->moves[0] = hash_move;
			fsm->has_hashmove = true;
			return 0;
		}
	}
	return -1;
}

static inline bool
move_fsm_has_any_killer(const struct move_fsm *fsm)
{
	/*
	 * killers are inserted into killers[0] first,
	 * then shifted to higher indices
	 * thus if killers[0] is empty -> no killers
	 */
	return fsm->killers[0] != 0;
}

static inline void
move_fsm_add_killer(struct move_fsm *fsm, move killer_move)
{
	for (unsigned i = ARRAY_LENGTH(fsm->killers) - 1; i > 0; --i)
		fsm->killers[i] = fsm->killers[i - 1];
	fsm->killers[0] = killer_move;
}

static inline unsigned
move_fsm_remaining(const struct move_fsm *fsm)
{
	return fsm->count - fsm->index;
}

static inline bool
move_fsm_done(const struct move_fsm *fsm)
{
	return fsm->index == fsm->count;
}

void move_fsm_reset(const struct position*, struct move_fsm*, unsigned i)
	attribute(nonnull);

#endif
