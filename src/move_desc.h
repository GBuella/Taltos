/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_MOVE_DESC_H
#define TALTOS_MOVE_DESC_H

#include "chess.h"

struct move_square_desc {
	int index;
	int piece;
	uint64_t rreach;
	uint64_t breach;
	uint64_t attacks;
	uint64_t attackers;
	int SEE_loss;
};

struct move_desc {
	move move;
	int value;

	int attack_count_delta;
	int discovered_attacks;
	bool direct_check;
	bool discovered_check;

	struct move_square_desc src_sq;
	struct move_square_desc dst_sq;

	int SEE_value;
};

static inline void
move_desc_setup(struct move_desc *desc)
{
	desc->move = 0;
}

void describe_move(struct move_desc*, const struct position*, move);

#endif
