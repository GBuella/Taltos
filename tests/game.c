/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include "tests.h"

#include <stdlib.h>

#include "game.h"
#include "str_util.h"

void
run_tests(void)
{
	struct game *game, *other;
	move move;

	game = game_create();
	if (game == NULL)
		abort();
	assert(game_turn(game) == white);
	assert(game_history_revert(game) != 0);
	assert(game_history_forward(game) != 0);
	assert(game_full_move_count(game) == 1);
	assert(game_half_move_count(game) == 0);
	move = create_move_pd(ind(rank_2, file_e), ind(rank_4, file_e));
	assert(game_append(game, move) == 0);
	other = game_copy(game);
	if (other == NULL)
		abort();
	assert(game_turn(game) == black);
	assert(game_turn(other) == black);
	assert(game_history_revert(other) == 0);
	game_destroy(other);
	move = create_move_pd(str_to_index("e7", black),
	    str_to_index("e5", black));
	assert(game_append(game, move) == 0);
	assert(game_turn(game) == white);
	game_destroy(game);
}
