
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "tests.h"

#include <assert.h>
#include <stdbool.h>

#include "position.h"
#include "eval.h"
#include "eval_terms.h"
#include "game.h"

static void
check_eval_symmetry(const struct position *pos)
{
	int value;
	int flipped_value;
	struct position flipped[1];
	struct eval_factors factors;
	struct eval_factors flipped_factors;

	position_flip(flipped, pos);

	assert(pos->pawn_attack_reach[0] ==
	    bswap(flipped->pawn_attack_reach[1]));
	assert(pos->pawn_attack_reach[1] ==
	    bswap(flipped->pawn_attack_reach[0]));
	assert(pos->half_open_files[0] ==
	    bswap(flipped->half_open_files[1]));
	assert(pos->half_open_files[1] ==
	    bswap(flipped->half_open_files[0]));

#define CHECK_TERM(func) \
	assert(func(pos) == bswap(opponent_##func(flipped))); \
	assert(func(flipped) == bswap(opponent_##func(pos)));

#define CHECK_BOOL_TERM(func) \
	assert(func(pos) == opponent_##func(flipped)); \
	assert(func(flipped) == opponent_##func(pos));

#define CHECK_INT_TERM(func) \
	assert(func(pos) == opponent_##func(flipped)); \
	assert(func(flipped) == opponent_##func(pos));

	CHECK_TERM(pawn_chains);
	CHECK_TERM(isolated_pawns);
	CHECK_TERM(blocked_pawns);
	CHECK_TERM(double_pawns);
	CHECK_TERM(backward_pawns);
	CHECK_TERM(outposts);
	CHECK_TERM(knight_outposts);
	CHECK_TERM(knight_reach_outposts);
	CHECK_TERM(passed_pawns);
	CHECK_TERM(rooks_on_half_open_files);
	CHECK_TERM(rooks_on_open_files);
	CHECK_TERM(rook_batteries);
	CHECK_TERM(pawns_on_center);
	CHECK_TERM(pawns_on_center4);
	CHECK_TERM(knight_center_attacks);
	CHECK_TERM(knight_center4_attacks);
	CHECK_TERM(bishop_center4_attacks);
	CHECK_BOOL_TERM(has_bishop_pair);
	assert(pawns_on_white(pos) == bswap(pawns_on_black(flipped)));
	assert(pawns_on_white(flipped) == bswap(pawns_on_black(pos)));
	assert(bishops_on_white(pos) ==
	    bswap(opponent_bishops_on_black(flipped)));
	assert(bishops_on_white(flipped) ==
	    bswap(opponent_bishops_on_black(pos)));
	assert(bishops_on_black(pos) ==
	    bswap(opponent_bishops_on_white(flipped)));
	assert(bishops_on_black(flipped) ==
	    bswap(opponent_bishops_on_white(pos)));
	CHECK_TERM(free_squares);
	CHECK_INT_TERM(non_pawn_material);
	assert(bishop_c1_is_trapped(pos) ==
	    opponent_bishop_c8_is_trapped(flipped));
	assert(bishop_f1_is_trapped(pos) ==
	    opponent_bishop_f8_is_trapped(flipped));
	assert(bishop_c1_is_trapped(flipped) ==
	    opponent_bishop_c8_is_trapped(pos));
	assert(bishop_f1_is_trapped(flipped) ==
	    opponent_bishop_f8_is_trapped(pos));
	assert(bishop_trapped_at_a7(pos) ==
	    opponent_bishop_trapped_at_a2(flipped));
	assert(bishop_trapped_at_a7(flipped) ==
	    opponent_bishop_trapped_at_a2(pos));
	assert(bishop_trapped_at_h7(pos) ==
	    opponent_bishop_trapped_at_h2(flipped));
	assert(bishop_trapped_at_h7(flipped) ==
	    opponent_bishop_trapped_at_h2(pos));
	assert(rook_a1_is_trapped(pos) == opponent_rook_a8_is_trapped(flipped));
	assert(rook_a1_is_trapped(flipped) == opponent_rook_a8_is_trapped(pos));
	assert(rook_h1_is_trapped(pos) == opponent_rook_h8_is_trapped(flipped));
	assert(rook_h1_is_trapped(flipped) == opponent_rook_h8_is_trapped(pos));
	assert(knight_cornered_a8(pos) == opponent_knight_cornered_a1(flipped));
	assert(knight_cornered_a8(flipped) == opponent_knight_cornered_a1(pos));
	assert(knight_cornered_h8(pos) == opponent_knight_cornered_h1(flipped));
	assert(knight_cornered_h8(flipped) == opponent_knight_cornered_h1(pos));

#undef CHECK_TERM
#undef CHECK_BOOL_TERM
#undef CHECK_INT_TERM

	value = eval(pos);
	flipped_value = eval(flipped);
	factors = compute_eval_factors(pos);
	flipped_factors = compute_eval_factors(flipped);

	assert(factors.material == -flipped_factors.material);
	assert(factors.basic_mobility == -flipped_factors.basic_mobility);
	assert(factors.pawn_structure == -flipped_factors.pawn_structure);
	assert(factors.rook_placement == -flipped_factors.rook_placement);
	assert(factors.bishop_placement == -flipped_factors.bishop_placement);
	assert(factors.knight_placement == -flipped_factors.knight_placement);
	assert(factors.passed_pawns == -flipped_factors.passed_pawns);
	assert(factors.center_control == -flipped_factors.center_control);
	assert(factors.king_safety == -flipped_factors.king_safety);
	assert(value == -flipped_value);
}

static void
test_tree_walk(const struct position *pos, unsigned depth)
{
	move moves[MOVE_ARRAY_LENGTH];

	if (!pos_has_ep_target(pos))
		check_eval_symmetry(pos);

	if (depth == 0)
		return;

	gen_moves(pos, moves);

	for (unsigned i = 0; moves[i] != 0; ++i) {
		struct position child[1];
		make_move(child, pos, moves[i]);
		test_tree_walk(child, depth - 1);
	}
}

void
run_tests(void)
{
	const struct game *g = parse_setboard_from_arg_file();

	setup_registers();
	test_tree_walk(game_current_position(g), 4);
}
