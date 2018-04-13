/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2018, Gabor Buella
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

#ifndef TALTOS_ZOBRIST_H
#define TALTOS_ZOBRIST_H

#include "chess.h"

static inline uint64_t
z_toggle_ep_file(uint64_t hash, int file)
{
	invariant(is_valid_file(file));

	extern const uint64_t zobrist_ep_file_value[8];

	return hash ^ zobrist_ep_file_value[file];
}

static inline uint64_t
z_toggle_pp(uint64_t hash, int index, enum piece p, enum player pl)
{
	invariant(ivalid(index));
	invariant(is_valid_piece(p));
	invariant(pl == white || pl == black);

	extern const uint64_t zobrist_random[14][64];

	return hash ^= zobrist_random[p + pl][index];
}

static inline uint64_t
z_toggle_sq(uint64_t hash, int index, int square)
{
	invariant(ivalid(index));
	invariant(is_valid_square(square));

	extern const uint64_t zobrist_random[14][64];

	return hash ^= zobrist_random[square][index];
}

static inline uint64_t
z_toggle_white_castle_queen_side(uint64_t hash)
{
	return hash ^ UINT64_C(0xF165B587DF898190);
}

static inline uint64_t
z_toggle_black_castle_queen_side(uint64_t hash)
{
	return hash ^ UINT64_C(0x1EF6E6DBB1961EC9);
}

static inline uint64_t
z_toggle_white_castle_king_side(uint64_t hash)
{
	return hash ^ UINT64_C(0x31D71DCE64B2C310);
}

static inline uint64_t
z_toggle_black_castle_king_side(uint64_t hash)
{
	return hash ^ UINT64_C(0xA57E6339DD2CF3A0);
}

#endif
