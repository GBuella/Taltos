/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#include <cstdint>

namespace taltos
{

constexpr int max_value = 30000;
constexpr int mate_value = max_value - 512;
constexpr int pawn_value = 100;
constexpr int knight_value = 300;
constexpr int bishop_value = 300;
constexpr int rook_value = 500;
constexpr int queen_value = 930;

// values are always expressed in centipans
static_assert(pawn_value == 100);

// lot of code assumes these
static_assert(knight_value > pawn_value);
static_assert(knight_value == bishop_value);
static_assert(rook_value > knight_value);
static_assert(queen_value > rook_value);

static inline bool is_value_valid(int value)
{
	return value >= -max_value && value <= max_value;
}

extern const std::array<int, 14> piece_value;

class position;

int eval(const position*);

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

int eval_threats(const position*);

struct eval_factors compute_eval_factors(const position*);

}

#endif
