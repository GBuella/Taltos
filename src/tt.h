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

#ifndef TALTOS_TT_H
#define TALTOS_TT_H

#include <atomic>
#include <array>
#include <vector>
#include <cstring>

#include "chess.h"

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
		return best_move_from == mfrom(m) and best_move_to == mto(m);
	}

	bool has_move() const
	{
		return best_move_from != 0 or best_move_to != 0;
	}

	bool operator==(const ht_entry& other) const
	{
		uint64_t raw, raw_other;
		std::memcpy(&raw, this, sizeof(raw));
		std::memcpy(&raw_other, &other, sizeof(raw_other));
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
		__builtin_prefetch(uintptr_t(table.data()) + (key & hash_mask));
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

}

#endif
