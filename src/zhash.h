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

#ifndef TALTOS_ZHASH_H
#define TALTOS_ZHASH_H

#include <cinttypes>

namespace taltos
{

struct zhash
{
	static const uint64_t random[14][64];
	uint64_t value;

	zhash()
	{
	}

	explicit constexpr zhash(uint64_t n):
		value(n)
	{
	}

	void toggle_ep_file(int file)
	{
		invariant(file >= 0 && file <= 7);

		static const uint64_t zobrist_value[8] = {
			UINT64_C(0x70CC73D90BC26E24), UINT64_C(0xE21A6B35DF0C3AD7),
			UINT64_C(0x003A93D8B2806962), UINT64_C(0x1C99DED33CB890A1),
			UINT64_C(0xCF3145DE0ADD4289), UINT64_C(0xD0E4427A5514FB72),
			UINT64_C(0x77C621CC9FB3A483), UINT64_C(0x67A34DAC4356550B) };

		value ^= zobrist_value[file];
	}

	void toggle_square(int i, int piece, int player)
	{
		value ^= random[piece + player][i];
	}

	void toggle_castle_queen_side_opponent()
	{
		value ^= UINT64_C(0x1EF6E6DBB1961EC9);
	}

	void toggle_castle_queen_side()
	{
		value ^= UINT64_C(0xF165B587DF898190);
	}

	void toggle_castle_king_side_opponent()
	{
		value ^= UINT64_C(0xA57E6339DD2CF3A0);
	}

	void toggle_castle_king_side()
	{
		value ^= UINT64_C(0x31D71DCE64B2C310);
	}

	void reset()
	{
		value = 0;
	}
};

class zhash_pair
{
	zhash value[2];
public:
	zhash_pair()
	{
	}

	zhash_pair flipped() const
	{
		zhash_pair result;
		result.value[0] = value[1];
		result.value[1] = value[0];
		return result;
	}

	void reset()
	{
		value[0].reset();
		value[1].reset();
	}

	uint64_t key() const
	{
		return value[0].value;
	}

	uint64_t key(int ep_file) const
	{
		zhash k = value[0];
		k.toggle_ep_file(ep_file);
		return k.value;
	}

	void toggle_square(int i, int piece, int player)
	{
		value[0].toggle_square(i, piece, player);
		value[1].toggle_square(flip_i(i), piece, opponent_of(player));
	}

	void toggle_castle_queen_side_opponent()
	{
		value[0].toggle_castle_queen_side_opponent();
		value[1].toggle_castle_queen_side();
	}

	void toggle_castle_queen_side()
	{
		value[0].toggle_castle_queen_side();
		value[1].toggle_castle_queen_side_opponent();
	}

	void toggle_castle_king_side_opponent()
	{
		value[0].toggle_castle_king_side_opponent();
		value[1].toggle_castle_king_side();
	}

	void toggle_castle_king_side()
	{
		value[0].toggle_castle_king_side();
		value[1].toggle_castle_king_side_opponent();
	}
};

}

#endif
