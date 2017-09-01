
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "tests.h"

#include <stdbool.h>
#include <string.h>

#include "position.h"
#include "game.h"

static void
check_zhash(const struct position *pos, enum player player)
{
	struct position temp;
	char fen[FEN_BUFFER_LENGTH];
	int ep_index;

	position_print_fen_full(pos, fen, 0, 1, 1, player);
	position_read_fen(&temp, fen, &ep_index, &player);

	assert(memcmp(pos->zhash, temp.zhash, sizeof(pos->zhash)) == 0);
}

static void
test_tree_walk(const struct position *pos, unsigned depth, enum player player)
{
	move moves[MOVE_ARRAY_LENGTH];

	check_zhash(pos, player);

	if (depth == 0)
		return;

	gen_moves(pos, moves);

	for (unsigned i = 0; moves[i] != 0; ++i) {
		struct position child[1];
		make_move(child, pos, moves[i]);
		test_tree_walk(child, depth - 1, opponent_of(player));
	}
}

void
run_tests(void)
{
	const struct game *g = parse_setboard_from_arg_file();

	test_tree_walk(game_current_position(g), 4, game_turn(g));
}
