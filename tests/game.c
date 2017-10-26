/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
