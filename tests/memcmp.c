
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "tests.h"

#include <stdbool.h>
#include <string.h>

#include "position.h"
#include "game.h"

static void
check_memcmp(const struct position *pos, enum player player)
{
	struct position temp[1];
	char fen[FEN_BUFFER_LENGTH];
	int ep_index;

	memset(temp, 0, sizeof(temp));
	position_print_fen_full(pos, fen, 0, 1, 1, player);
	position_read_fen(temp, fen, &ep_index, &player);

	assert(memcmp(pos->board, temp->board, sizeof(pos->board)) == 0);
	assert(pos->king_attack_map == temp->king_attack_map);
	assert(pos->king_danger_map == temp->king_danger_map);
	assert(pos->ep_index == temp->ep_index);
	assert(pos->occupied == temp->occupied);
	assert(pos->king_index == temp->king_index);
	assert(memcmp(pos->attack, temp->attack, sizeof(pos->attack)) == 0);
	assert(memcmp(pos->sliding_attacks, temp->sliding_attacks,
	    sizeof(pos->sliding_attacks)) == 0);
	assert(memcmp(pos->map, temp->map, sizeof(pos->map)) == 0);
	assert(memcmp(pos->half_open_files, temp->half_open_files,
	    sizeof(pos->half_open_files)) == 0);
	assert(memcmp(pos->pawn_attack_reach, temp->pawn_attack_reach,
	    sizeof(pos->pawn_attack_reach)) == 0);
	assert(memcmp(pos->rays, temp->rays, sizeof(pos->rays)) == 0);
	assert(memcmp(pos->rays_bishops, temp->rays_bishops,
	    sizeof(pos->rays_bishops)) == 0);
	assert(memcmp(pos->rays_rooks, temp->rays_rooks,
	    sizeof(pos->rays_rooks)) == 0);
	assert(pos->cr_king_side == temp->cr_king_side);
	assert(pos->cr_queen_side == temp->cr_queen_side);
	assert(pos->material_value == temp->material_value);
	assert(pos->cr_opponent_king_side == temp->cr_opponent_king_side);
	assert(pos->cr_opponent_queen_side == temp->cr_opponent_queen_side);
	assert(pos->opponent_material_value == temp->opponent_material_value);
	assert(memcmp(pos->zhash, temp->zhash, sizeof(pos->zhash)) == 0);
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

	test_tree_walk(game_current_position(g), 4, game_turn(g));
}
