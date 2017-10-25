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

#include "macros.h"

#include "transposition_table.h"
#include "search.h"
#include "util.h"
#include "eval.h"
#include "trace.h"

#include <cstring>
#include <stdexcept>

#include "str_util.h"

using std::atomic_uint64_t;

namespace taltos
{

const unsigned transposition_table::min_size_mb = 1;
const unsigned transposition_table::max_size_mb = 0x1000;

static unsigned round_down(unsigned n)
{
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n = n + 1;
	n >>= 1;

	return n;
}

static size_t
compute_size(unsigned megabytes)
{
	if (megabytes < transposition_table::min_size_mb)
		throw std::invalid_argument("TT size");
	if (megabytes > transposition_table::max_size_mb)
		throw std::invalid_argument("TT size");

	size_t size = round_down(megabytes);
	if (size * 1024 < size)
		throw std::overflow_error("TT size");
	size *= 1024;
	if (size * 1024 < size)
		throw std::overflow_error("TT size");

	return size;
}

size_t transposition_table::entry_count() const
{
	return table.size() * (sizeof(bucket) / sizeof(ht_entry));
}

size_t transposition_table::size() const
{
	return table.size() * sizeof(bucket);
}

transposition_table::transposition_table(unsigned megabytes)
{
	resize(megabytes);
}

static ht_entry load_entry(const atomic_uint64_t* storage)
{
	ht_entry entry;
	uint64_t raw = storage->load(std::memory_order_relaxed);
	std::memcpy(&entry, &raw, sizeof(raw));
	return entry;
}

static void overwrite_entry(atomic_uint64_t* storage, ht_entry new_entry)
{
	uint64_t raw;
	std::memcpy(&raw, &new_entry, sizeof(raw));
	storage->store(raw, std::memory_order_relaxed);
}

static void update_entry(atomic_uint64_t* storage, ht_entry old_entry, ht_entry new_entry)
{
	if (old_entry.has_move() and not new_entry.has_move()) {
		new_entry.best_move_from = old_entry.best_move_from;
		new_entry.best_move_to = old_entry.best_move_to;
	}
	overwrite_entry(storage, new_entry);
}

ht_entry transposition_table::lookup(const struct position* pos) const
{
	uint64_t index = pos->zhash[0] & hash_mask;
	const bucket* bucket = &table[index];

	for (const auto& e : bucket->entries) {
		auto entry = load_entry(&e);
		if (entry.key_upper22 == pos->zhash[0] >> 42)
			return entry;
	}

	return ht_null;
}

void transposition_table::update(const struct position* pos, ht_entry new_entry)
{
	uint64_t index = pos->zhash[0] & hash_mask;
	bucket* bucket = &table[index];
	atomic_uint64_t* overwrite_candidate = &bucket->entries[0];
	int overwrite_candidate_depth = 1000;
	new_entry.generation = 3;

	for (auto& e : bucket->entries) {
		auto existing_entry = load_entry(&e);
		if (existing_entry.key_upper22 == pos->zhash[0] >> 42) {
			update_entry(&e, existing_entry, new_entry);
			return;
		}

		int depth = existing_entry.depth - (3 - existing_entry.generation) * PLY * 2;
		if (depth < overwrite_candidate_depth) {
			overwrite_candidate = &e;
			overwrite_candidate_depth = depth;
		}
	}

	overwrite_entry(overwrite_candidate, new_entry);
}

void transposition_table::resize(unsigned megabytes)
{
	size_t new_size = compute_size(megabytes);
	if (size() == new_size)
		return;

	decltype(table) new_table(new_size / sizeof(bucket));
	table = std::move(new_table);
	for (auto& b : table) {
		for (auto& e : b.entries)
			e.store(0, std::memory_order_relaxed);
	}
	hash_mask = table.size() - 1;
}

}
