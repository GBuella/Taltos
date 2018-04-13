/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2017-2018, Gabor Buella
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
position_memcmp(const struct position *pos0)
{
	struct position pos1[1];
	char fen[FEN_BUFFER_LENGTH];

	memset(pos1, 0, sizeof(pos1));
	position_print_fen(fen, pos0);
	position_read_fen(fen, pos1);

	assert(pos0->turn == pos1->turn);
	assert(pos0->opponent == pos1->opponent);
	assert(pos0->half_move_counter == pos1->half_move_counter);
	assert(pos0->full_move_counter == pos1->full_move_counter);
	assert(memcmp(pos0->board, pos1->board, sizeof(pos0->board)) == 0);
	assert(pos0->king_attack_map == pos1->king_attack_map);
	assert(pos0->king_danger_map == pos1->king_danger_map);
	assert(pos0->ep_index == pos1->ep_index);
	assert(pos0->occupied == pos1->occupied);
	assert(pos0->white_ki == pos1->white_ki);
	assert(pos0->black_ki == pos1->black_ki);
	assert(memcmp(pos0->attack, pos1->attack, sizeof(pos0->attack)) == 0);
	assert(memcmp(pos0->sliding_attacks, pos1->sliding_attacks,
	    sizeof(pos0->sliding_attacks)) == 0);
	assert(memcmp(pos0->map, pos1->map, sizeof(pos0->map)) == 0);
	assert(memcmp(pos0->half_open_files, pos1->half_open_files,
	    sizeof(pos0->half_open_files)) == 0);
	assert(memcmp(pos0->pawn_attack_reach, pos1->pawn_attack_reach,
	    sizeof(pos0->pawn_attack_reach)) == 0);
	assert(memcmp(pos0->rq, pos1->rq, sizeof(pos0->rq)) == 0);
	assert(memcmp(pos0->bq, pos1->bq, sizeof(pos0->bq)) == 0);
	assert(memcmp(pos0->rays[0], pos1->rays[0], sizeof(pos0->rays[0])) == 0);
	assert(memcmp(pos0->rays[1], pos1->rays[1], sizeof(pos0->rays[1])) == 0);
	assert(pos0->zhash == pos1->zhash);
	assert(pos0->cr_white_king_side == pos1->cr_white_king_side);
	assert(pos0->cr_white_queen_side == pos1->cr_white_queen_side);
	assert(pos0->material_value[white] == pos1->material_value[white]);
	assert(pos0->cr_black_king_side == pos1->cr_black_king_side);
	assert(pos0->cr_black_queen_side == pos1->cr_black_queen_side);
	assert(pos0->material_value[black]== pos1->material_value[black]);
	assert(pos0->king_pins[0] == pos1->king_pins[0]);
	assert(pos0->king_pins[1] == pos1->king_pins[1]);
	assert(pos0->nb[0] == pos1->nb[0]);
	assert(pos0->nb[1] == pos1->nb[1]);
	assert(pos0->undefended[0] == pos1->undefended[0]);
	assert(pos0->undefended[1] == pos1->undefended[1]);
	assert(pos0->all_kings == pos1->all_kings);
	assert(pos0->all_knights == pos1->all_knights);
	assert(pos0->all_rq == pos1->all_rq);
	assert(pos0->all_bq == pos1->all_bq);
	assert(memcmp(pos0->hanging, pos1->hanging,
		      sizeof(pos0->hanging)) == 0);
	assert(pos0->hanging_map == pos1->hanging_map);
}

static void
check_memcmp(const struct position *pos)
{
	struct position temp[1];

	memset(temp, 0, sizeof(temp));

	position_memcmp(pos);

	if (is_in_check(pos) || position_has_en_passant_target(pos))
		return;

	assert(position_flip(temp, pos) == 0);

	position_memcmp(temp);
}

static void
test_tree_walk(const struct position *pos, unsigned depth)
{
	struct move moves[MOVE_ARRAY_LENGTH];

	check_memcmp(pos);

	if (depth == 0)
		return;

	gen_moves(pos, moves);

	for (unsigned i = 0; !is_null_move(moves[i]); ++i) {
		struct position child[1];
		memset(child, 0, sizeof(child));
		make_move(child, pos, moves[i]);
		test_tree_walk(child, depth - 1);
	}
}

void
run_tests(void)
{
	const struct game *g = parse_setboard_from_arg_file();

	test_tree_walk(game_current_position(g), 3);
}
