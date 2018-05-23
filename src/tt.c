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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

#include "tt.h"
#include "util.h"
#include "trace.h"

enum {

#ifdef TALTOS_TT_GENERATION_BIT_COUNT
	generation_bit_count = (TALTOS_TT_GENERATION_BIT_COUNT),
#else
	generation_bit_count = 4,
#endif

	min_log2_size = 5,
	max_log2_size = 26,
	key_bit_count  = (39 - generation_bit_count),
//	depth_delta_bits = 5,
//	depth_delta_max = ((1 << depth_delta_bits) - 1)
};

static_assert(min_log2_size > 1, "invalid TT min size");
static_assert(min_log2_size < max_log2_size,
	      "TT min should be smalller than TT max");

static_assert(generation_bit_count > 0,
	      "invalid bit count for TT generation field");

static_assert(generation_bit_count < 39,
	      "invalid bit count for TT generation field");

struct entry {
	uint32_t key;
	int16_t value;
	uint32_t generation : 7;
	bool contains_exact_value : 1;

	uint32_t reserved : 8;

	int16_t value2;
	uint32_t depth : 8;
	uint32_t depth2 : 8;

	uint32_t mg_index : 8;
	uint32_t mg_index2 : 8;
	uint32_t reserved3 : 5;
	bool contains_value : 1;
	bool contains_second_value : 1;
	bool no_null_flag : 1;

	uint32_t reserved2 : 8;
};

static_assert(sizeof(struct entry) == 2 * sizeof(uint64_t), "layout");
static_assert(offsetof(struct entry, value2) == sizeof(uint64_t), "layout");

struct raw_entry {
	// TODO: use atomic_uin64_t + use reserved fields as thread ids
	uint64_t raw[2];
};

struct bucket {
	struct raw_entry entries[4];
};

static struct entry
load_entry(const struct raw_entry *raw)
{
	uint64_t ints[2];
	ints[0] = raw->raw[0]; // TODO: atomic load
	ints[1] = raw->raw[1];
	struct entry entry;

	tmemcpy(&entry, ints, sizeof(entry));

	return entry;
}

static void
store_entry(struct raw_entry *raw, struct entry entry)
{
	uint64_t ints[2];

	tmemcpy(ints, &entry, sizeof(entry));

	raw->raw[0] = ints[0]; // TODO: atomic store
	raw->raw[1] = ints[1];
}

static bool
is_empty_entry(struct entry entry)
{
	uint64_t x;
	tmemcpy(&x, &entry, sizeof(x));
	return x == 0;
}

struct tt {
	unsigned long bucket_count;
	struct bucket *table;
	unsigned long usage;
	unsigned log2_size;
	unsigned current_generation;
};

size_t
tt_slot_count(const struct tt *tt)
{
	return tt->bucket_count * ARRAY_LENGTH(tt->table[0].entries);
}

size_t
tt_usage(const struct tt *tt)
{
	return tt->usage;
}

size_t
tt_size(const struct tt *tt)
{
	return tt->bucket_count * sizeof(tt->table[0]);
}

unsigned
tt_min_size_mb(void)
{
	return 1;
}

unsigned
tt_max_size_mb(void)
{
	return (((size_t)1 << max_log2_size) * sizeof(struct bucket))
	    / (1024 * 1024);
}

struct tt*
tt_create(unsigned log2_size)
{
	tracef("%s %u", __func__, log2_size);

	struct tt *tt;

	if (log2_size < min_log2_size || log2_size > max_log2_size)
		return NULL;

	tt = xmalloc(sizeof *tt);
	tt->bucket_count = (((size_t)1) << log2_size);
	tt->log2_size = log2_size;
	tt->table = aligned_alloc(alignof(struct bucket), tt_size(tt));
	if (tt->table == NULL) {
		free(tt);
		return NULL;
	}
	tt_clear(tt);
	return tt;
}

static unsigned
get_log2_size(uintmax_t megabytes)
{
	static const size_t multiplier = 1024 * 1024 / sizeof(struct bucket);

	if (megabytes == 0)
		return 0;

	if ((megabytes & (megabytes - 1)) != 0)
		return 0;

	if (megabytes * multiplier < megabytes)
		return 0;

	megabytes *= multiplier;

	unsigned result = 0;

	while (megabytes != (1u << result))
		++result;

	return result;
}

bool
tt_is_mb_size_valid(unsigned megabytes)
{
	return megabytes >= tt_min_size_mb()
	    && megabytes <= tt_max_size_mb()
	    && get_log2_size(megabytes) != 0;
}

struct tt*
tt_create_mb(unsigned megabytes)
{
	tracef("%s %umb", __func__, megabytes);

	return tt_create(get_log2_size(megabytes));
}

struct tt*
tt_resize(struct tt *tt, unsigned log2_size)
{
	tracef("%s %u", __func__, log2_size);

	unsigned long bucket_count = (1lu << log2_size);

	if (tt == NULL)
		return tt_create(log2_size);

	if (tt->log2_size == log2_size)
		return tt;

	tt->log2_size = log2_size;

	struct bucket *table;

	table = aligned_alloc(alignof(struct bucket),
	    bucket_count * sizeof(table[0]));
	if (table == NULL)
		return NULL;

	xaligned_free(tt->table);
	tt->table = table;
	tt->bucket_count = bucket_count;
	tt_clear(tt);
	return tt;
}

struct tt*
tt_resize_mb(struct tt *tt, unsigned megabytes)
{
	tracef("%s %umb", __func__, megabytes);

	return tt_resize(tt, get_log2_size(megabytes));
}

void
tt_clear(struct tt *tt)
{
	trace(__func__);

	memset((void*)(tt->table), 0, tt_size(tt));
	tt->usage = 0;
	tt->current_generation = 0;
}

void
tt_destroy(struct tt *tt)
{
	if (tt != NULL) {
		xaligned_free((void*)tt->table);
		free(tt);
	}
}

static struct bucket*
find_bucket(const struct tt *tt, uint64_t key)
{
	return tt->table + (key & (tt->bucket_count - 1));
}

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH
void
tt_prefetch(const struct tt *tt, uint64_t hash)
{
	const char *address = (const char*)find_bucket(tt, hash);

	prefetch(address);
}
#endif

static uint32_t
ekey(uint64_t key)
{
	return key >> 32;
}

void
tt_lookup(struct tt *tt, uint64_t key, int d,
	  struct tt_lookup_result *result)
{
	struct bucket *bucket = find_bucket(tt, key);
	uint8_t depth = clamp(d, 0, 100);

	key = ekey(key);
	result->move_count = 0;
	result->found_value = result->found_exact_value = false;
	result->no_null_flag = false;

	for (size_t i = 0; i < ARRAY_LENGTH(bucket->entries); ++i) {
		struct entry entry = load_entry(bucket->entries + i);

		if (entry.key != key)
			continue;

		result->no_null_flag = entry.no_null_flag;

		if (entry.mg_index != no_move) {
			result->moves[0] = entry.mg_index;
			if (entry.mg_index2 != no_move) {
				result->moves[1] = entry.mg_index;
				result->move_count = 2;
			}
			else {
				result->move_count = 1;
			}
		}

		if (entry.depth >= depth) {
			result->found_value = true;
			result->found_exact_value = entry.contains_exact_value;
			result->value = entry.value;
			result->depth = entry.depth;
		}
		else if (entry.depth2 >= depth) {
			result->found_value = true;
			result->found_exact_value = false;
			result->value = entry.value2;
			result->depth = entry.depth2;
		}

		if (entry.generation != tt->current_generation) {
			entry.generation = tt->current_generation;
			store_entry(bucket->entries + i, entry);
		}

		return;
	}
}

static unsigned
candidate_value(struct entry entry)
{
	unsigned value = entry.generation + entry.depth;

	if (entry.contains_exact_value)
		value += 3;

	if (entry.depth2 != 0)
		++value;

	return value;
}

static struct entry
update_to_exact_value(struct entry entry, int16_t value, uint8_t depth)
{
	entry.value = value;
	entry.depth = depth;
	entry.contains_exact_value = true;

	if (entry.depth2 <= depth || entry.value2 >= value)
		entry.depth2 = 0;

	return entry;
}

static struct entry
update_exact_values(struct entry entry, int16_t value, uint8_t depth)
{
	if (entry.depth > depth)
		return entry;

	return update_to_exact_value(entry, value, depth);
}

static struct entry
add_lower_bound(struct entry entry, int16_t value, uint8_t depth)
{
	if (!entry.contains_value) {
		entry.value = value;
		entry.depth = depth;
		entry.contains_value = true;
		return entry;
	}

	if (entry.depth2 > depth)
		return entry;

	if (entry.depth > depth && entry.depth2 <= depth) {
		entry.contains_second_value = true;
		entry.depth2 = depth;
		entry.value2 = value;
	}

	return entry;
}

static struct entry
update_lower_bound(struct entry entry, int16_t value, uint8_t depth)
{
	if (!entry.contains_value) {
		entry.value = value;
		entry.depth = depth;
		entry.contains_value = true;
		return entry;
	}

	if (entry.depth <= depth) {
		entry.depth = depth;
		entry.value = value;
		if (entry.depth2 <= depth)
			entry.depth2 = 0;
	}
	else if (entry.depth2 <= depth) {
		entry.contains_second_value = true;
		entry.depth2 = depth;
		entry.value2 = value;
	}

	return entry;
}

static struct entry
update_entry_move(struct entry entry, uint8_t mg_index)
{
	if (mg_index != no_move && mg_index != entry.mg_index) {
		entry.mg_index2 = entry.mg_index;
		entry.mg_index = mg_index;
	}

	return entry;
}

static struct entry
update_entry_value(struct entry entry, int16_t value, bool is_exact_value,
		   uint8_t depth)
{
	if (is_exact_value && entry.contains_exact_value)
		entry = update_exact_values(entry, value, depth);
	else if (is_exact_value && !entry.contains_exact_value)
		entry = update_to_exact_value(entry, value, depth);
	else if (entry.contains_exact_value && !is_exact_value)
		entry = add_lower_bound(entry, value, depth);
	else
		entry = update_lower_bound(entry, value, depth);

	return entry;
}

static void
insert(struct tt *tt, uint64_t key, int16_t value, bool insert_value,
       bool is_exact_value, bool no_null_flag, uint8_t mg_index, int d)
{
	struct bucket *bucket = find_bucket(tt, key);
	struct raw_entry *replacement_candidate = NULL;
	unsigned best_value = UINT_MAX;
	uint8_t depth = clamp(d, 0, 100);

	key = ekey(key);

	for (size_t i = 0; i < ARRAY_LENGTH(bucket->entries); ++i) {
		struct raw_entry *raw = bucket->entries + i;
		struct entry entry = load_entry(raw);

		if (is_empty_entry(entry)) {
			replacement_candidate = raw;
			break;
		}
		else if (entry.key == key) {
			entry.generation = tt->current_generation;
			entry.no_null_flag = entry.no_null_flag || no_null_flag;
			entry = update_entry_move(entry, mg_index);
			if (insert_value)
				entry = update_entry_value(entry, value,
							   is_exact_value,
							   depth);
			store_entry(raw, entry);

			return;
		}
		else {
			unsigned value = candidate_value(entry);
			if (value < best_value) {
				best_value = value;
				replacement_candidate = raw;
			}
		}
	}

	if (replacement_candidate != NULL) {
		struct entry entry = {
			.key = key,
			.generation = tt->current_generation,
			.contains_exact_value = is_exact_value,
			.contains_value = insert_value,
			.contains_second_value = false,
			.value = value,
			.depth = depth,
			.no_null_flag = no_null_flag,
			.reserved = 0,
			.value2 = 0,
			.depth2 = 0,
			.mg_index = mg_index,
			.mg_index2 = no_move,
			.reserved2 = 0,
			.reserved3 = 0
		};
		store_entry(replacement_candidate, entry);
	}
}

void
tt_insert_exact_value(struct tt *tt, uint64_t key, int16_t value, int depth,
		      bool no_null_flag)
{
	insert(tt, key, value, true, true, no_null_flag, no_move, depth);
}

void
tt_insert_exact_valuem(struct tt *tt, uint64_t key, uint8_t mg_index,
		       int16_t value, int depth, bool no_null_flag)
{
	invariant(mg_index != no_move);
	insert(tt, key, value, true, true, no_null_flag, mg_index, depth);
}

void
tt_insert_lower_bound(struct tt *tt, uint64_t key, int16_t value, int depth,
		      bool no_null_flag)
{
	insert(tt, key, value, true, false, no_null_flag, no_move, depth);
}

void
tt_insert_lower_boundm(struct tt *tt, uint64_t key, uint8_t mg_index,
		       int16_t value, int depth, bool no_null_flag)
{
	invariant(mg_index != no_move);
	insert(tt, key, value, true, false, no_null_flag, mg_index, depth);
}

void
tt_insert_move(struct tt *tt, uint64_t key, uint8_t mg_index, bool no_null_flag)
{
	invariant(mg_index != no_move);
	insert(tt, key, 0, false, false, no_null_flag, mg_index, 0);
}

void
tt_generation_step(struct tt *tt)
{
	tt->current_generation++;
}
