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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "macros.h"

#include "hash.h"
#include "search.h"
#include "util.h"
#include "eval.h"
#include "trace.h"
#include "util.h"

#include "str_util.h"

namespace taltos
{

#ifndef HT_MIN_SIZE
#define HT_MIN_SIZE 5
#endif

#ifndef HT_MAX_SIZE
#define HT_MAX_SIZE 26
#endif


/*
 * The bits in the lower 32 bits, not used for storing the move,
 * or the value_type.
 */
#define COUNTER_MASK \
	((uint64_t)((uint32_t)~(MOVE_MASK | VALUE_TYPE_MASK | HT_NO_NULL_FLAG)))

static_assert((MOVE_MASK & VALUE_TYPE_MASK) == 0, "layout");
static_assert((MOVE_MASK & HT_NO_NULL_FLAG) == 0, "layout");
static_assert((VALUE_TYPE_MASK & HT_NO_NULL_FLAG) == 0, "layout");

#define HASH_MASK (~COUNTER_MASK)

struct slot {
	alignas(16) uint64_t hash_key;
	uint64_t entry;
};

#ifndef FRESH_SLOT_COUNT
#define FRESH_SLOT_COUNT 2
#endif

#ifndef DEEP_SLOT_COUNT
#define DEEP_SLOT_COUNT 6
#endif

static_assert((FRESH_SLOT_COUNT > 0),
		"FRESH_SLOT_COUNT expected to be a positive integer");

static_assert((DEEP_SLOT_COUNT > 0),
		"DEEP_SLOT_COUNT expected to be a positive integer");

static_assert((DEEP_SLOT_COUNT % 2) == 0,
		"DEEP_SLOT_COUNT expected to be an even number");

#define SLOT_COUNT (FRESH_SLOT_COUNT + DEEP_SLOT_COUNT)

static_assert(((SLOT_COUNT & (SLOT_COUNT - 1)) == 0),
		"FRESH_SLOT_COUNT + DEEP_SLOT_COUNT "
		"expected to be a power of two");

struct bucket {
	alignas(SLOT_COUNT * sizeof(struct slot)) struct slot slots[SLOT_COUNT];
};

struct hash_table {
	unsigned long bucket_count;
	struct bucket *table;
	unsigned long usage;
	unsigned log2_size;
};

std::size_t
ht_slot_count(const struct hash_table *ht)
{
	return ht->bucket_count * ARRAY_LENGTH(ht->table[0].slots);
}

std::size_t
ht_usage(const struct hash_table *ht)
{
	return ht->usage;
}

std::size_t
ht_size(const struct hash_table *ht)
{
	return ht->bucket_count * sizeof(ht->table[0]);
}

unsigned
ht_min_size_mb(void)
{
	return 1;
}

unsigned
ht_max_size_mb(void)
{
	return (((std::size_t)1 << HT_MAX_SIZE) * sizeof(struct bucket))
	    / (1024 * 1024);
}

struct hash_table*
ht_create(unsigned log2_size)
{
	tracef("%s %u", __func__, log2_size);

	struct hash_table *ht;

	if (log2_size < HT_MIN_SIZE || log2_size > HT_MAX_SIZE)
		return nullptr;

	ht = new hash_table;
	ht->bucket_count = (((std::size_t)1) << log2_size);
	ht->log2_size = log2_size;
	ht->table = (bucket*) alloc_align64(ht_size(ht));
	if (ht->table == nullptr) {
		delete ht;
		return nullptr;
	}
	ht_clear(ht);
	return ht;
}

static unsigned
get_log2_size(uintmax_t megabytes)
{
	static const std::size_t multiplier = 1024 * 1024 / sizeof(struct bucket);

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
ht_is_mb_size_valid(unsigned megabytes)
{
	return megabytes >= ht_min_size_mb()
	    && megabytes <= ht_max_size_mb()
	    && get_log2_size(megabytes) != 0;
}

struct hash_table*
ht_create_mb(unsigned megabytes)
{
	tracef("%s %umb", __func__, megabytes);

	return ht_create(get_log2_size(megabytes));
}

struct hash_table*
ht_resize(struct hash_table *ht, unsigned log2_size)
{
	tracef("%s %u", __func__, log2_size);

	unsigned long bucket_count = (1lu << log2_size);

	if (ht == nullptr)
		return ht_create(log2_size);

	if (ht->log2_size == log2_size)
		return ht;

	ht->log2_size = log2_size;

	struct bucket *table;

	table = (bucket*) alloc_align64(bucket_count * sizeof(table[0]));
	if (table == nullptr)
		return nullptr;

	free_align64(ht->table);
	ht->table = table;
	ht->bucket_count = bucket_count;
	ht_clear(ht);
	return ht;
}

struct hash_table*
ht_resize_mb(struct hash_table *ht, unsigned megabytes)
{
	tracef("%s %umb", __func__, megabytes);

	return ht_resize(ht, get_log2_size(megabytes));
}

void
ht_clear(struct hash_table *ht)
{
	trace(__func__);

	memset((void*)(ht->table), 0, ht_size(ht));
	ht->usage = 0;
}

void
ht_destroy(struct hash_table *ht)
{
	if (ht != nullptr) {
		free_align64((void*)ht->table);
		delete ht;
	}
}

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH
void
ht_prefetch(const struct hash_table *ht, uint64_t hash)
{
	prefetch(ht->table + (hash & (ht->bucket_count - 1)));
}
#endif

static bool
hash_match(const struct slot *slot, const uint64_t hash[])
{
	return ((slot->hash_key ^ hash[1]) & HASH_MASK) == 0;
}

static bool
thread_id_match(const struct slot *slot)
{
	(void) slot;
	return true;
	// todo: return ((slot->hash_key ^ slot->entry) & COUNTER_MASK) == 0;
}

static bool
move_ok(ht_entry e, const struct position *pos)
{
	if (!ht_has_move(e))
		return true;

	move m = ht_move(e);
	if (pos_piece_at(pos, mfrom(m)) == 0)
		return false;
	if (pos_player_at(pos, mfrom(m)) != 0)
		return false;
	return ((pos_piece_at(pos, mto(m)) == 0)
	    || (pos_player_at(pos, mto(m)) == 1));
}

ht_entry
ht_lookup_fresh(const struct hash_table *ht,
		const struct position *pos)
{
	struct bucket *bucket = ht->table + (pos->zhash[0] & (ht->bucket_count - 1));

	unsigned index = DEEP_SLOT_COUNT;
	index += ((pos->zhash[0] >> ht->log2_size) % FRESH_SLOT_COUNT);

	struct slot slot = bucket->slots[index];

	if (thread_id_match(&slot)
	    && hash_match(&slot, pos->zhash)
	    && move_ok(slot.entry, pos))
		return slot.entry;
	return 0;
}

ht_entry
ht_lookup_deep(const struct hash_table *ht,
		const struct position *pos,
		int depth,
		int beta)
{
	ht_entry best = 0;
	struct bucket *bucket = ht->table + (pos->zhash[0] & (ht->bucket_count - 1));

	for (std::size_t i = 0; i < DEEP_SLOT_COUNT; ++i) {
		struct slot slot = bucket->slots[i];
		if (thread_id_match(&slot)
		    && hash_match(&slot, pos->zhash)
		    && move_ok(slot.entry, pos)) {
			if (ht_depth(slot.entry) >= depth) {
				if (ht_value_type(slot.entry) == vt_exact)
					return slot.entry;
				if (ht_value_type(slot.entry) == vt_lower_bound
				    && ht_value(slot.entry) >= beta)
					return slot.entry;
			}
			if (ht_depth(slot.entry) >= ht_depth(best)) {
				best = slot.entry;
			}
		}
	}
	return best;
}

/*
 * todo:
 * static uint64_t
 * increment_counter(uint64_t counter)
 * {
 * return (counter - COUNTER_MASK) & COUNTER_MASK;
 * }
 */

static void
overwrite_slot(struct slot *dst, const uint64_t hash[], ht_entry entry)
{
	// todo:
	// uint64_t counter = increment_counter(dst->hash_key & COUNTER_MASK);
	uint64_t counter = 0;
	dst->hash_key = (hash[1] & HASH_MASK) | counter;
	dst->entry = entry | counter;
}

static void
write_fresh_slot(struct hash_table *ht,
		struct bucket *bucket,
		const uint64_t hash[],
		ht_entry entry)
{
	unsigned index = DEEP_SLOT_COUNT;
	index += ((hash[0] >> ht->log2_size) % FRESH_SLOT_COUNT);

	overwrite_slot(bucket->slots + index, hash, entry);
}

void
ht_pos_insert(struct hash_table *ht,
		const struct position *pos,
		ht_entry entry)
{
	struct bucket *bucket;
	const uint64_t *hash;
	if (!ht_is_set(entry))
		return;

	hash = pos->zhash;
	bucket = ht->table + (hash[0] & (ht->bucket_count - 1));

	write_fresh_slot(ht, bucket, hash, entry);

	unsigned depth_min_index = 0;
	int depth_min = 0x10000;

	for (unsigned i = 0; i < DEEP_SLOT_COUNT / 2; ++i) {
		struct slot tslot = bucket->slots[i];
		if (hash_match(&tslot, hash)
		    && move_ok(tslot.entry, pos)
		    && (ht_value_type(tslot.entry) == ht_value_type(entry))) {
			if (ht_depth(tslot.entry) <= ht_depth(entry))
				overwrite_slot(bucket->slots + i, hash, entry);
			return;
		}
		if (ht_depth(tslot.entry) < depth_min) {
			depth_min = ht_depth(tslot.entry);
			depth_min_index = i;
		}
	}
	if (depth_min < ht_depth(entry)) {
		if (bucket->slots[depth_min_index].hash_key == 0)
			ht->usage++;

		overwrite_slot(bucket->slots + depth_min_index, hash, entry);
	}
}

void
ht_extract_pv(const struct hash_table *ht,
		const struct position *pos,
		int depth,
		move pv[],
		int value)
{
	move moves[MOVE_ARRAY_LENGTH];
	ht_entry e;
	struct position child[1];

	pv[0] = 0;
	if (depth <= 0)
		return;
	(void) gen_moves(pos, moves);
	e = ht_lookup_deep(ht, pos, depth, max_value);
	if (!ht_is_set(e))
		return;
	if (!ht_has_move(e))
		return;
	if (ht_depth(e) < depth && depth > PLY)
		return;
	if (ht_value_type(e) != vt_exact || ht_value(e) != value)
		return;
	for (move *m = moves; *m != ht_move(e); ++m)
		if (*m == 0)
			return;
	pv[0] = ht_move(e);
	position_make_move(child, pos, pv[0]);
	ht_extract_pv(ht, child, depth - PLY, pv + 1, -value);
}

static void
slot_swap(struct hash_table *ht, struct slot *slot)
{
	if (slot->hash_key != 0) {
		slot[DEEP_SLOT_COUNT / 2] = *slot;
		if (ht_depth(slot->entry) < 99) {
			ht->usage--;
			slot->hash_key = 0;
			slot->entry = 0;
		}
	}
}

void
ht_swap(struct hash_table *ht)
{
	trace(__func__);

	for (unsigned i = 0; i != ht->bucket_count; ++i) {
		struct bucket *bucket = ht->table + i;
		for (unsigned si = 0; si < DEEP_SLOT_COUNT / 2; ++si)
			slot_swap(ht, bucket->slots + si);
	}
}

}
