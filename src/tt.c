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
#include "search.h"
#include "util.h"
#include "eval.h"
#include "trace.h"

#include "str_util.h"

#ifdef TALTOS_TT_MIN_SIZE
#define TT_MIN_SIZE (TALTOS_TT_MIN_SIZE)
#else
#define TT_MIN_SIZE 5
#endif

#ifdef TALTOS_TT_MAX_SIZE
#define TT_MAX_SIZE (TALTOS_TT_MAX_SIZE)
#else
#define TT_MAX_SIZE 26
#endif

static_assert(TT_MIN_SIZE > 1, "invalid TT min size");
static_assert(TT_MIN_SIZE < TT_MAX_SIZE,
	      "TT min should be smalller than TT max");

#ifdef TALTOS_TT_GENERATION_BIT_COUNT
#define TT_GENERATION_BIT_COUNT (TALTOS_TT_GENERATION_BIT_COUNT)
#else
#define TT_GENERATION_BIT_COUNT 4
#endif

static_assert(TT_GENERATION_BIT_COUNT > 0,
	      "invalid bit count for TT generation field");

static_assert(TT_GENERATION_BIT_COUNT < 28,
	      "invalid bit count for TT generation field");

#define TT_KEY_BIT_COUNT (29 - TT_KEY_BIT_COUNT)

/************************

struct value_entry {
	int16_t value;
	uint16_t depth : 14;
	bool no_null : 1;
	bool is_exact_value : 1;
};

static_assert(sizeof(struct value_entry) == sizeof(uint32_t), "layout");

struct packed_value_entry {
	int16_t value : 10;
	uint16_t depth : 7;
	int16_t value2_delta : 9;
	uint16_t depth2_delta : 6;
};

static_assert(sizeof(struct packed_value_entry) == sizeof(uint16_t), "layout");

static_assert(sizeof(struct move) == sizeof(uint32_t), "layout");

struct x {
	int16_t value;
	uint8_t move_index : 7;
	bool no_null : 1;
	uint8_t depth : 8;
};

struct internal_entry {
	uint32_t key : TT_KEY_BIT_COUNT;
	uint32_t generation : TT_GENERATION_BIT_COUNT;
	bool contains_packed_values : 1;
	bool contains_exact_value : 1;
	union {
		struct value_entry value;
		struct packed_value_entry packed_values[2];
		struct move best_move;
	};
};

static_assert(sizeof(struct internal_entry) == sizeof(uint64_t), "layout");

************************/

struct slot {
	alignas(16) uint64_t key;
	uint64_t entry;
};

enum {
	bucket_slot_count = 8
};

struct bucket {
	alignas(bucket_slot_count * sizeof(struct slot))
		struct slot slots[bucket_slot_count];
};

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
	return tt->bucket_count * bucket_slot_count;
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
	return (((size_t)1 << TT_MAX_SIZE) * sizeof(struct bucket))
	    / (1024 * 1024);
}

struct tt*
tt_create(unsigned log2_size)
{
	tracef("%s %u", __func__, log2_size);

	struct tt *tt;

	if (log2_size < TT_MIN_SIZE || log2_size > TT_MAX_SIZE)
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
	prefetch(address + 64);
}
#endif

static bool
move_ok(struct tt_entry e, const struct position *pos)
{
	if (!tt_has_move(e))
		return true;

	if (pos_piece_at(pos, e.best_move_from) == 0)
		return false;

	if (pos_player_at(pos, e.best_move_from) != pos->turn)
		return false;

	if (pos_piece_at(pos, e.best_move_to) == 0)
		return true;

	return pos_player_at(pos, e.best_move_to) != pos->turn;
}

struct tt_entry
tt_lookup(const struct tt *tt, const struct position *pos)
{
	struct bucket *bucket = find_bucket(tt, pos->zhash);

	for (int i = 0; i < bucket_slot_count; ++i) {
		if (bucket->slots[i].key != pos->zhash)
			continue;

		struct tt_entry e = int_to_tt_entry(bucket->slots[i].entry);
		if (!move_ok(e, pos))
			continue;

		if (e.generation != tt->current_generation) {
			e.generation = tt->current_generation;
			bucket->slots[i].entry = tt_entry_to_int(e);
		}

		return e;
	}

	return tt_null();
}

static void
overwrite_slot(struct slot *slot, const struct position *pos, struct tt_entry e)
{
	slot->key = pos->zhash;
	slot->entry = tt_entry_to_int(e);
}

static unsigned
candidate_value(struct tt_entry e)
{
	unsigned value = e.generation + e.depth * 2;
	if (tt_has_exact_value(e))
		value += 4;
	if (tt_has_move(e))
		++value;
	return value;
}

static bool
should_overwrite_at_same_key(struct tt_entry old, struct tt_entry new)
{
	if (old.depth + 1 * PLY < new.depth)
		return true;

	if (old.depth <= new.depth)
		return tt_has_exact_value(new) || !tt_has_exact_value(old);

	if (old.depth < new.depth + 2 * PLY)
		return tt_has_exact_value(new) && !tt_has_exact_value(old);

	return false;
}

void
tt_pos_insert(struct tt *tt, const struct position *pos, struct tt_entry e)
{
	struct bucket *bucket = find_bucket(tt, pos->zhash);

	struct slot *replace_candidate = NULL;
	e.generation = tt->current_generation;
	unsigned best_candidate_value = 0;

	for (int i = 0; i < bucket_slot_count; ++i) {
		struct tt_entry old_e = int_to_tt_entry(bucket->slots[i].entry);

		if (bucket->slots[i].key == pos->zhash) {
			if (should_overwrite_at_same_key(old_e, e)) {
				if (tt_has_move(old_e) && !tt_has_move(e))
					e = tt_set_move(e, tt_move(old_e));
				overwrite_slot(bucket->slots + i, pos, e);
			} else {
				if (tt_has_move(e) && !tt_has_move(old_e))
					old_e = tt_set_move(old_e, tt_move(e));
				old_e.generation = tt->current_generation;
				overwrite_slot(bucket->slots + i, pos, old_e);
			}

			return;
		}
	}

	for (int i = 0; i < bucket_slot_count; ++i) {
		struct tt_entry old_e = int_to_tt_entry(bucket->slots[i].entry);

		if (!tt_entry_is_set(old_e)) {
			overwrite_slot(bucket->slots + i, pos, e);
			tt->usage++;
			return;
		}

		unsigned value = candidate_value(old_e);
		if (replace_candidate == NULL || value < best_candidate_value) {
			replace_candidate = bucket->slots + i;
			best_candidate_value = value;
		}
	}

	if (replace_candidate != NULL)
		overwrite_slot(replace_candidate, pos, e);
}

void
tt_generation_step(struct tt *tt)
{
	tt->current_generation++;
}

void
tt_extract_pv(const struct tt *tt, const struct position *pos,
	      int depth, struct move pv[], int value)
{
	struct move moves[MOVE_ARRAY_LENGTH];
	struct tt_entry e;
	struct position child[1];

	pv[0] = null_move();
	if (depth <= 0)
		return;
	(void) gen_moves(pos, moves);
	e = tt_lookup(tt, pos);
	if (!tt_entry_is_set(e))
		return;
	if (!tt_has_move(e))
		return;
	if (e.depth < depth && depth > PLY)
		return;
	if (!tt_has_exact_value(e))
		return;
	if (e.value != value)
		return;
	for (struct move *m = moves; !move_eq(*m, tt_move(e)); ++m)
		if (is_null_move(*m))
			return;
	pv[0] = tt_move(e);
	make_move(child, pos, pv[0]);
	tt_extract_pv(tt, child, depth - PLY, pv + 1, -value);
}
