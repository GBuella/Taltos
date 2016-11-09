
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
#include "exchange.h"

static unsigned random_hint_index;
static char move_stack[1024];
static char *move_stack_p = move_stack;
static enum player turn;

static int
mat_value(const struct position *pos)
{
	return pos->material_value - pos->opponent_material_value;
}

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

static int
SEE_negamax_verify(const struct position *pos, int to)
{
	int best = mat_value(pos);

	move moves[MOVE_ARRAY_LENGTH];

	unsigned count = gen_moves(pos, moves);

	move best_capture = 0;
	int best_capturing_value = 10000;
	for (unsigned i = 0; i < count; ++i) {
		move m = moves[i];
		if (mto(m) != to)
			continue;

		int capturing_value;

		if (mresultp(m) == king)
			capturing_value = 9999;
		else
			capturing_value =
			    piece_value[(unsigned)pos->board[mfrom(m)]];

		if (capturing_value < best_capturing_value) {
			best_capture = m;
			best_capturing_value = capturing_value;
		}
		else if (capturing_value == best_capturing_value) {
			if (mresultp(m) == knight
			    && mresultp(best_capture) == bishop)
				best_capture = m;
			else if (mresultp(m) == bishop
			    && mresultp(best_capture) == knight)
				continue;
			else if (mfrom(m) < mfrom(best_capture))
				best_capture = m;
		}
	}

	if (best_capture == 0)
		return best;

	move_stack_append(best_capture);
	struct position child[1];
	setup_registers();
	make_move(child, pos, best_capture);
	int value = -SEE_negamax_verify(child, flip_i(to));
	move_stack_pop();
	if (value > best)
		best = value;

	return best;
}

static int
SEE_move_verify(const struct position *pos, move m)
{
	struct position child[1];
	setup_registers();
	make_move(child, pos, m);

	int new_value = -SEE_negamax_verify(child, flip_i(mto(m)));

	return new_value - mat_value(pos);
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

	move_order_setup(move_order, pos, false);
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
		move_order_pick_next(move_order, pos);
		move m = mo_current_move(move_order);
		move_stack_append(m);
		if (mtype(m) != mt_castle_kingside
		    && mtype(m) != mt_castle_queenside
		    && mtype(m) != mt_en_passant) {
			int see_value = SEE_move(pos, m);
			int see_value_check = SEE_move_verify(pos, m);
			if (see_value != see_value_check) {
				fprintf(stderr,
				    "SEE mismatch at %s : %d vs %d\n",
				    move_stack, see_value, see_value_check);
				abort();
			}
		}
		check_moves[i] = m;
		setup_registers();
		make_move(child, pos, m);
		assert(move_gives_check(pos, m) == is_in_check(child));
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
