
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "perft.h"
#include "chess.h"
#include "chess.h"
#include "position.h"
#include "engine.h"
#include "move_order.h"
#include "util.h"
#include "eval.h"
#include "str_util.h"

struct divide_info {
	struct position pos;
	unsigned depth;
	move moves[MOVE_ARRAY_LENGTH];
	move *m;
	char str[32];
	enum player turn;
	bool is_ordered;
};

static uintmax_t
do_qperft(const struct position *pos, unsigned depth)
{
	uintmax_t n = 0;
	move moves[MOVE_ARRAY_LENGTH];
	struct position child[1];

	if (depth == 0)
		return 1;
	if (depth == 1)
		return gen_moves(pos, moves);
	(void) gen_moves(pos, moves);
	for (move *i = moves; *i != 0; ++i) {
		make_move(child, pos, *i);
		n += do_qperft(child, depth - 1);
	}
	return n;
}

static uintmax_t
do_perft(const struct position *pos, unsigned depth)
{
	uintmax_t n;
	move moves[MOVE_ARRAY_LENGTH];
	struct position child[1];

	if (depth == 0)
		return 1;
	(void) gen_moves(pos, moves);
	n = 0;
	for (move *i = moves; *i != 0; ++i) {
		make_move(child, pos, *i);
		n += do_perft(child, depth - 1);
	}
	return n;
}

uintmax_t
perft(const struct position *pos, unsigned depth)
{
	assert(depth <= MAX_PLY);
	return do_perft(pos, depth);
}

uintmax_t
qperft(const struct position *pos, unsigned depth)
{
	assert(depth <= MAX_PLY);
	return do_qperft(pos, depth);
}


uintmax_t
perft_ordered(const struct position *pos, unsigned depth)
{
	assert(depth <= MAX_PLY);

	struct move_order move_order[1];
	uintmax_t n;
	struct position child[1];

	if (depth == 0)
		return 1;

	move_order_setup(move_order, pos, false, 0);

	if (depth == 1)
		return move_order->count;

	if (move_order->count == 0)
		return 0;

	if (move_order->count > 16)
		move_order_add_hint(move_order, move_order->moves[12], 1);

	if (move_order->count > 13)
		move_order_add_killer(move_order, move_order->moves[10]);
	else
		move_order->killers[0] = 0;

	move_order->killers[1] = 0;
	n = 0;
	do {
		move_order_pick_next(move_order);
		make_move(child, pos, mo_current_move(move_order));
		n += perft_ordered(child, depth - 1);
	} while (!move_order_done(move_order));
	return n;
}

struct divide_info*
divide_init(const struct position *pos, unsigned depth,
		enum player turn, bool ordered)
{
	assert(depth > 0 && depth <= MAX_PLY);
	assert(turn == white || turn == black);

	struct divide_info *dinfo;

	dinfo = xaligned_alloc(pos_alignment, sizeof *dinfo);
	dinfo->depth = depth;
	dinfo->turn = turn;
	dinfo->is_ordered = ordered;
	dinfo->pos = *pos;
	(void) gen_moves(pos, dinfo->moves);
	dinfo->m = dinfo->moves;
	return dinfo;
}

const char*
divide(struct divide_info *dinfo, enum move_notation_type mn)
{
	if (*dinfo->m == 0)
		return NULL;

	struct position t;
	uintmax_t n;

	char *str;

	str = print_move(&dinfo->pos, *dinfo->m, dinfo->str, mn, dinfo->turn);
	make_move(&t, &dinfo->pos, *dinfo->m);
	if (dinfo->is_ordered) {
		n = perft_ordered(&t, dinfo->depth - 1);
	}
	else {
		n = qperft(&t, dinfo->depth - 1);
	}
	(void) sprintf(str, " %" PRIuMAX, n);
	++dinfo->m;
	return dinfo->str;
}

void
divide_destruct(struct divide_info *dinfo)
{
	xaligned_free(dinfo);
}
