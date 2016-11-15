
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

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
test_tree_walk(const struct position *pos, unsigned depth)
{
	struct move_order move_order[1];
	struct position child[1];
	move check_moves[MOVE_ARRAY_LENGTH];
	move original_moves[MOVE_ARRAY_LENGTH];
	bool move_present[MOVE_ARRAY_LENGTH] = {0};
	unsigned count;
	move hint_move;

	if (depth == 0)
		return;

	move_order_setup(move_order, pos, false, false, 0);
	count = move_order->count;
	if (count == 0)
		return;

	for (unsigned i = 0; i < count; ++i)
		original_moves[i] = move_order->moves[i];

	unsigned hindex = random_hint_index % (count + 1);
	if (hindex > 0) {
		hint_move = original_moves[hindex - 1];
		move_order_add_hint(move_order, hint_move, 1);
	}
	else {
		hint_move = 0;
	}

	step_hint_index();

	if (move_order->count > 13)
		move_order_add_killer(move_order, move_order->moves[10]);
	else
		move_order->killers[0] = 0;

	move_order->killers[1] = 0;

	unsigned i = 0;
	do {
		move_order_pick_next(move_order);
		move m = mo_current_move(move_order);
		move_stack_append(m);
		check_moves[i] = m;
		setup_registers();
		make_move(child, pos, m);
		test_tree_walk(child, depth - 1);
		++i;
		move_stack_pop();
	} while (!move_order_done(move_order));

	assert(i == count);
	assert(count == move_order->count);
	if (hint_move != 0)
		assert(move_order->moves[0] == hint_move);

	for (unsigned i = 0; i < count; ++i) {
		unsigned j;
		for (j = 0; j < count; ++j) {
			if (original_moves[j] == check_moves[i]) {
				assert(!move_present[j]);
				move_present[j] = true;
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
	test_tree_walk(game_current_position(g), 4);
}
