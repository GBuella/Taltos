
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_EVAL_H
#define TALTOS_EVAL_H

#include <assert.h>
#include <stdint.h>

enum {
	max_value = 0x1fffe,
	mate_value = 0x14000,
	pawn_value = 100,
	knight_value = 300,
	bishop_value = 300,
	rook_value = 500,
	queen_value = 930
};

// values are always expressed in centipans
static_assert(pawn_value == 100, "pawn_value must be 100 centipawns");

// lot of code assumes these
static_assert(knight_value > pawn_value,
	"knight_value must be larger than pawn_value");
static_assert(knight_value == bishop_value,
	"knight_value must be equal to bishop_value");
static_assert(rook_value > knight_value,
	"rook_value must be larger than knight_value");
static_assert(queen_value > rook_value,
	"queen_value must be larger than rook_value");

static inline bool
value_bounds(int value)
{
	return value >= -max_value && value <= max_value;
}

extern const int piece_value[14];

struct position;

int eval(const struct position*) attribute(nonnull);

struct eval_factors {
	int material;
	int basic_mobility;
	int pawn_structure;
	int rook_placement;
	int bishop_placement;
	int knight_placement;
	int passed_pawns;
	int center_control;
	int king_safety;
	int threats;
};

int eval_threats(const struct position *pos)
	attribute(nonnull);

struct eval_factors compute_eval_factors(const struct position*)
	attribute(nonnull);

#endif
