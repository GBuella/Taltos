
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_EVAL_H
#define TALTOS_EVAL_H

#include <stdint.h>

enum {
	max_value = 30000,
	mate_value = (max_value - MAX_PLY),
	pawn_value = 100,
	knight_value = 300,
	bishop_value = 301,
	rook_value = 500,
	queen_value = 900
};

extern const int piece_value[14];

struct position;

int eval(const struct position*) attribute(nonnull);

struct eval_factors {
	int material;
	int middle_game;
	int end_game;
	int basic_mobility;
	int pawn_structure;
	int rook_placement;
	int bishop_placement;
	int knight_placement;
	int passed_pawns;
	int center_control;
	int king_fortress;
};

struct eval_factors compute_eval_factors(const struct position*)
	attribute(nonnull);

#endif
