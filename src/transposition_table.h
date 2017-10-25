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

#ifndef TALTOS_TRANSPOSITION_TABLE_H
#define TALTOS_TRANSPOSITION_TABLE_H

#include "move.h"

#include <atomic>
#include <vector>

namespace taltos
{

struct ht_entry {
	int16_t value;
	uint64_t best_move_from : 6;
	uint64_t best_move_to : 6;
	uint64_t is_lower_bound : 1;
	uint64_t is_upper_bound : 1;
	uint64_t depth : 8;
	uint64_t no_null : 1;
	uint64_t generation : 2;
	uint64_t key_upper22 : 22;

	bool has_value() const
	{
		return is_lower_bound or is_upper_bound;
	}

	bool has_exact_value() const
	{
		return is_lower_bound and is_upper_bound;
	}

	bool matches_move(move m) const
	{
		return best_move_from == m.from and best_move_to == m.to;
	}

	bool has_move() const
	{
		return best_move_from != 0 or best_move_to != 0;
	}

	bool operator==(const ht_entry& other) const
	{
		uint64_t raw, raw_other;
		memcpy(&raw, this, sizeof(raw));
		memcpy(&raw_other, &other, sizeof(raw_other));
		return raw == raw_other;
	}
};

static_assert(sizeof(ht_entry) == sizeof(std::atomic_uint64_t));

class transposition_table
{
	struct bucket {
		std::array<std::atomic_uint64_t, 8> entries;
	};
	std::vector<bucket> table;

	uint64_t hash_mask;

public:
	static const unsigned min_size_mb;
	static const unsigned max_size_mb;

	transposition_table(unsigned megabytes);
	transposition_table(const transposition_table&) = delete;
	transposition_table& operator=(const transposition_table&) = delete;

	ht_entry lookup(const class position*) const;
	void update(const class position*, ht_entry);

	void prefetch(uint64_t key) const
	{
#ifdef __GNUC__
		const char* address = (const char*)(table.data() + (key & hash_mask));
		__builtin_prefetch(address);
#else
		(void) key;
#endif
	}

	void clear();

	void resize(unsigned megabytes);
	void new_generation();
	size_t size() const;
	size_t usage() const;
	size_t entry_count() const;
};

constexpr ht_entry ht_null{0, 0, 0, 0, 0, 0, 0, 0, 0};



static inline uint64_t
z_toggle_ep_file(uint64_t hash, int file)
{
	invariant(file >= 0 && file <= 7);

	static const uint64_t zobrist_value[8] = {
		UINT64_C(0x70CC73D90BC26E24), UINT64_C(0xE21A6B35DF0C3AD7),
		UINT64_C(0x003A93D8B2806962), UINT64_C(0x1C99DED33CB890A1),
		UINT64_C(0xCF3145DE0ADD4289), UINT64_C(0xD0E4427A5514FB72),
		UINT64_C(0x77C621CC9FB3A483), UINT64_C(0x67A34DAC4356550B) };

	return hash ^ zobrist_value[file];
}

static inline uint64_t
z_toggle_sq(uint64_t hash, int i, int piece, int player)
{
	invariant(is_valid_index(i));

	extern const uint64_t z_random[14][64];

	return hash ^= z_random[piece + player][i];
}

static inline void
z2_toggle_sq(uint64_t hash[2], int i, int piece, int player)
{
	invariant(is_valid_index(i));

	extern const uint64_t z_random[14][64];

	hash[0] ^= z_random[piece + player][i];
	hash[1] ^= z_random[opponent_of(piece + player)][flip_i(i)];
}

static inline uint64_t
z_toggle_castle_queen_side_opponent(uint64_t hash)
{
	return hash ^ UINT64_C(0x1EF6E6DBB1961EC9);
}

static inline uint64_t
z_toggle_castle_queen_side(uint64_t hash)
{
	return hash ^ UINT64_C(0xF165B587DF898190);
}

static inline uint64_t
z_toggle_castle_king_side_opponent(uint64_t hash)
{
	return hash ^ UINT64_C(0xA57E6339DD2CF3A0);
}

static inline uint64_t
z_toggle_castle_king_side(uint64_t hash)
{
	return hash ^ UINT64_C(0x31D71DCE64B2C310);
}

static inline void
z2_toggle_castle_queen_side_opponent(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_queen_side_opponent(hash[0]);
	hash[1] = z_toggle_castle_queen_side(hash[1]);
}

static inline void
z2_toggle_castle_queen_side(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_queen_side(hash[0]);
	hash[1] = z_toggle_castle_queen_side_opponent(hash[1]);
}

static inline void
z2_toggle_castle_king_side_opponent(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_king_side_opponent(hash[0]);
	hash[1] = z_toggle_castle_king_side(hash[1]);
}

static inline void
z2_toggle_castle_king_side(uint64_t hash[2])
{
	hash[0] = z_toggle_castle_king_side(hash[0]);
	hash[1] = z_toggle_castle_king_side_opponent(hash[1]);
}

static inline void
prefetch_z2_xor_move(move m)
{
#ifdef __GNUC__
	alignas(16) extern uint64_t zhash_xor_table[64 * 64 * 8 * 8 * 8][2];

	__builtin_prefetch(zhash_xor_table + m.index(), 0, 0);
#else
	(void) m;
#endif
}

static inline void
z2_xor_move(uint64_t hash[2], move m)
{
	alignas(16) extern uint64_t zhash_xor_table[64 * 64 * 8 * 8 * 8][2];

	hash[0] ^= zhash_xor_table[m.index()][0];
	hash[1] ^= zhash_xor_table[m.index()][1];
}

}

#endif
