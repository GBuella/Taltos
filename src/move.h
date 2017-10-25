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

#ifndef TALTOS_MOVE_H
#define TALTOS_MOVE_H

#include "chess.h"
#include "bitboard.h"
#include "position.h"

#include <cstring>

namespace taltos
{

constexpr int mt_general = 0;
constexpr int mt_pawn_double_push = 1;
constexpr int mt_castle_kingside = 2;
constexpr int mt_castle_queenside = 3;
constexpr int mt_en_passant = 4;
constexpr int mt_promotion = 5;

constexpr bool
is_valid_mt(int t)
{
	return (t == mt_general)
	    || (t == mt_pawn_double_push)
	    || (t == mt_castle_kingside)
	    || (t == mt_castle_queenside)
	    || (t == mt_en_passant)
	    || (t == mt_promotion);
}

class alignas(4) move {
public:
	uint32_t from : 6;
	uint32_t to : 6;
private:
	uint32_t result_ : 3;
	uint32_t captured_ : 3;
public:
	uint32_t type : 3;
private:
	uint32_t padding : 32 - 6 - 6 - 3 - 3 - 3;

public:

	int result() const
	{
		return result_ << 1;
	}

	int captured() const
	{
		return captured_ << 1;
	}

	constexpr move():
		from(0),
		to(0),
		result_(0),
		captured_(0),
		type(0),
		padding(0)
	{
	}

	constexpr move(int type):
		from(e1),
		to(type == mt_castle_kingside ? g1 : c1),
		result_(king >> 1),
		captured_(0),
		type(type),
		padding(0)
	{
	}

	move(int from, int to, int result_piece, int captured = 0, int type = mt_general):
		from(from),
		to(to),
		result_(result_piece >> 1),
		captured_(captured >> 1),
		type(type),
		padding(0)
	{
		invariant(is_valid_index(from));
		invariant(is_valid_index(to));
		invariant(is_valid_piece(result_piece));
		invariant(captured == 0 or is_valid_piece(captured));
		invariant(is_valid_mt(type));
	}

	uint32_t index() const
	{
		invariant(padding == 0);
		uint32_t i;
		std::memcpy(&i, this, sizeof(i));
		return i;
	}

	bool is_set() const
	{
		return index() != 0;
	}

	bool is_null() const
	{
		return index() == 0;
	}

	bool operator==(const move& other) const
	{
		return index() == other.index();
	}

	bitboard mask() const
	{
		switch (type) {
			case mt_castle_queenside:
				return bb(a1, c1, d1, e1);
			case mt_castle_kingside:
				return bb(e1, f1, g1, h1);
			case mt_en_passant:
				return bb(from, to, to + south);
			default:
				return bb(from, to);
		}
	}

	bool is_promotion() const
	{
		return type == mt_promotion;
	}

	bool is_under_promotion() const
	{
		return is_promotion() and result() != queen;
	}

	bool is_capture() const
	{
		return captured_ != 0;
	}
};

static_assert(sizeof(move) == sizeof(uint32_t));

constexpr move null_move;

constexpr move castle_king_side(mt_castle_kingside);
constexpr move castle_queen_side(mt_castle_queenside);

}

#endif
