/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2017, Gabor Buella
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

#include <stdbool.h>
#include <string.h>

#include "position.h"
#include "game.h"

static void
position_memcmp(const struct position *pos0, enum player player)
{
	struct position pos1[1];
	char fen[FEN_BUFFER_LENGTH];
	int ep_index;

	memset(pos1, 0, sizeof(pos1));
	position_print_fen_full(pos0, fen, 0, 1, 1, player);
	position_read_fen(pos1, fen, &ep_index, &player);

	assert(memcmp(pos0->board, pos1->board, sizeof(pos0->board)) == 0);
	assert(pos0->king_attack_map == pos1->king_attack_map);
	assert(pos0->king_danger_map == pos1->king_danger_map);
	assert(pos0->ep_index == pos1->ep_index);
	assert(pos0->occupied == pos1->occupied);
	assert(pos0->ki == pos1->ki);
	assert(pos0->opp_ki == pos1->opp_ki);
	assert(memcmp(pos0->attack, pos1->attack, sizeof(pos0->attack)) == 0);
	assert(memcmp(pos0->sliding_attacks, pos1->sliding_attacks,
	    sizeof(pos0->sliding_attacks)) == 0);
	assert(memcmp(pos0->map, pos1->map, sizeof(pos0->map)) == 0);
	assert(memcmp(pos0->half_open_files, pos1->half_open_files,
	    sizeof(pos0->half_open_files)) == 0);
	assert(memcmp(pos0->pawn_attack_reach, pos1->pawn_attack_reach,
	    sizeof(pos0->pawn_attack_reach)) == 0);
	assert(memcmp(pos0->rays[0], pos1->rays[0], sizeof(pos0->rays[0])) == 0);
	assert(memcmp(pos0->rays[1], pos1->rays[1], sizeof(pos0->rays[1])) == 0);
	assert(pos0->cr_king_side == pos1->cr_king_side);
	assert(pos0->cr_queen_side == pos1->cr_queen_side);
	assert(pos0->material_value == pos1->material_value);
	assert(pos0->cr_opponent_king_side == pos1->cr_opponent_king_side);
	assert(pos0->cr_opponent_queen_side == pos1->cr_opponent_queen_side);
	assert(pos0->opponent_material_value == pos1->opponent_material_value);
	assert(memcmp(pos0->zhash, pos1->zhash, sizeof(pos0->zhash)) == 0);
	assert(pos0->king_pins[0] == pos1->king_pins[0]);
	assert(pos0->king_pins[1] == pos1->king_pins[1]);
	assert(pos0->undefended[0] == pos1->undefended[0]);
	assert(pos0->undefended[1] == pos1->undefended[1]);
	assert(memcmp(pos0->hanging, pos1->hanging,
		      sizeof(pos0->hanging)) == 0);
	assert(pos0->hanging_map == pos1->hanging_map);
}

static void
check_memcmp(const struct position *pos, enum player player)
{
	struct position temp[1];

	memset(temp, 0, sizeof(temp));

	position_memcmp(pos, player);

	if (is_in_check(pos) || pos_has_ep_target(pos))
		return;

	position_flip(temp, pos);

	position_memcmp(temp, player);
}

static void
test_tree_walk(const struct position *pos, unsigned depth, enum player player)
{
	move moves[MOVE_ARRAY_LENGTH];

	check_memcmp(pos, player);

	if (depth == 0)
		return;

	gen_moves(pos, moves);

	for (unsigned i = 0; moves[i] != 0; ++i) {
		struct position child[1];
		memset(child, 0, sizeof(child));
		make_move(child, pos, moves[i]);
		test_tree_walk(child, depth - 1, opponent_of(player));
	}
}

void
run_tests(void)
{
	const struct game *g = parse_setboard_from_arg_file();

	test_tree_walk(game_current_position(g), 3, game_turn(g));
}
