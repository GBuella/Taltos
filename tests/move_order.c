/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include "tests.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "move_order.h"
#include "position.h"
#include "game.h"
#include "eval.h"

static unsigned random_hint_index;
static char move_stack[1024];
static char *move_stack_p = move_stack;
static enum player turn;

struct data {
	struct move_order move_order[1];
	struct position pos[1];
	move check_moves[MOVE_ARRAY_LENGTH];
	move original_moves[MOVE_ARRAY_LENGTH];
	bool move_present[MOVE_ARRAY_LENGTH];
};

static void
move_stack_append(move m)
{
	*move_stack_p++ = ' ';
	move_stack_p = print_coor_move(m, move_stack_p, turn);
	turn = opponent_of(turn);
}

static void
move_stack_pop(void)
{
	while (*move_stack_p != ' ')
		--move_stack_p;
	*move_stack_p = '\0';
	turn = opponent_of(turn);
}

static void
step_hint_index(void)
{
	random_hint_index = (random_hint_index * 33 + 1) % 1024;
}

static void
test_tree_walk(struct data *data, unsigned depth)
{
	unsigned count;
	move hint_move;

	if (depth == 0)
		return;

	memset(data->move_present, 0, sizeof(data->move_present));

	move_order_setup(data->move_order, data->pos, false, 0);
	count = data->move_order->count;
	if (count == 0)
		return;

	for (unsigned i = 0; i < count; ++i)
		data->original_moves[i] = data->move_order->moves[i];

	unsigned hindex = random_hint_index % (count + 1);
	if (hindex > 0) {
		hint_move = data->original_moves[hindex - 1];
		move_order_add_hint(data->move_order, hint_move, 1);
	}
	else {
		hint_move = 0;
	}

	step_hint_index();

	if (data->move_order->count > 13)
		move_order_add_killer(data->move_order,
				      data->move_order->moves[10]);
	else
		data->move_order->killers[0] = 0;

	data->move_order->killers[1] = 0;

	unsigned i = 0;
	do {
		move_order_pick_next(data->move_order);
		move m = mo_current_move(data->move_order);
		if (hint_move != 0 && i == 0)
			assert(m == hint_move);
		move_stack_append(m);
		data->check_moves[i] = m;
		make_move(data[1].pos, data->pos, m);
		test_tree_walk(data + 1, depth - 1);
		++i;
		move_stack_pop();
	} while (!move_order_done(data->move_order));

	assert(i == count);
	assert(count == data->move_order->count);

	for (unsigned i = 0; i < count; ++i) {
		unsigned j;
		for (j = 0; j < count; ++j) {
			if (data->original_moves[j] == data->check_moves[i]) {
				assert(!data->move_present[j]);
				data->move_present[j] = true;
				break;
			}
		}
		assert(j != count);
	}
}

void
run_tests(void)
{
	const struct game *g = parse_setboard_from_arg_file();

	turn = game_turn(g);
	move_stack[0] = ' ';
	move_stack_p = move_stack + 1;

	static struct data data[5];
	data[0].pos[0] = *(game_current_position(g));
	test_tree_walk(data, 3);
}
