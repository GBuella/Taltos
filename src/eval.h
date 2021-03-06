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

#ifndef TALTOS_EVAL_H
#define TALTOS_EVAL_H

#include <assert.h>
#include <stdint.h>

enum {
	max_value = 30000,
	mate_value = (max_value - MAX_PLY),
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
