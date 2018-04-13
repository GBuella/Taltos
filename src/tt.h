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

#ifndef TALTOS_TT_H
#define TALTOS_TT_H

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "chess.h"

struct tt;

#define TT_GENERATION_BITS 10
#define TT_DEPTH_BITS 8

#define TT_ENTRY_MAX_DEPTH ((1u << TT_DEPTH_BITS) - 1)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

enum { tt_entry_value_count = 3 };

/*
 * Disable pedantic warnings here. ISO says:
 * "A bit-field shall have a type that is a qualified or unqualified version of
 * _Bool, signed int, unsigned int, or some other implementation-defined type.
 * It is implementation-defined whether atomic types are permitted."
 *
 * Thus gcc-4.8 complains about uint64_t bit-fields:
 *
 * taltos/src/tt.h:43:2: error: type of bit-field best_move_from is a GCC extension [-Werror=pedantic]
 *  uint64_t best_move_from : 6;
 *   ^
 */

struct tt_entry {
	int16_t value;
	uint64_t best_move_from : 6;
	uint64_t best_move_to : 6;
	uint64_t best_move_result : 4;
	uint64_t best_move_captured : 4;
	uint64_t best_move_type : 3;
	uint64_t is_lower_bound : 1;
	uint64_t is_upper_bound : 1;
	uint64_t depth : TT_DEPTH_BITS;
	uint64_t no_null : 1;
	uint64_t generation : TT_GENERATION_BITS;
	uint64_t reserved : (64 - 16 - 6 - 6 - 4 - 4 - 3 - 1 - 1 - 1
			     - TT_DEPTH_BITS - TT_GENERATION_BITS);
};

#pragma GCC diagnostic pop

static_assert(sizeof(struct tt_entry) == sizeof(uint64_t), "layout error");
static_assert(alignof(struct tt_entry) == alignof(uint64_t), "layout error");



static inline struct tt_entry
tt_set_move(struct tt_entry e, struct move m)
{
	e.best_move_from = m.from;
	e.best_move_to = m.to;
	e.best_move_result = m.result;
	e.best_move_captured = m.captured;
	e.best_move_type = m.type;

	return e;
}

static inline struct move
tt_move(struct tt_entry entry)
{
	return (struct move) {
		.from = entry.best_move_from,
		.to = entry.best_move_to,
		.result = entry.best_move_result,
		.captured = entry.best_move_captured,
		.type = entry.best_move_type,
		.reserved = 0
	};
}

static inline bool
tt_has_move(struct tt_entry e)
{
	return e.best_move_result != 0;
}

static inline struct tt_entry
tt_set_no_move(struct tt_entry e)
{
	e.best_move_from = 0;
	e.best_move_to = 0;
	e.best_move_result = 0;
	return e;
}

static inline bool
tt_has_exact_value(struct tt_entry e)
{
	return e.is_lower_bound && e.is_upper_bound;
}

static inline bool
tt_has_value(struct tt_entry entry)
{
	return entry.is_lower_bound || entry.is_upper_bound;
}

static inline uint64_t
tt_entry_to_int(struct tt_entry e)
{
	uint64_t value;
	char *dst = (void*)&value;
	const char *src = (const void*)&e;

	for (size_t i = 0; i < sizeof(value); ++i)
		dst[i] = src[i];

	return value;
}

static inline struct tt_entry
int_to_tt_entry(uint64_t value)
{
	struct tt_entry e;
	char *dst = (void*)&e;
	const char *src = (const void*)&value;

	for (size_t i = 0; i < sizeof(value); ++i)
		dst[i] = src[i];

	
	return e;
}

static inline bool
tt_entry_is_set(struct tt_entry e)
{
	return tt_entry_to_int(e) != 0;
}

static inline struct tt_entry
tt_null(void)
{
	return int_to_tt_entry(0);
}



struct tt *tt_create(unsigned log2_size);

struct tt *tt_create_mb(unsigned megabytes);

struct tt *tt_resize(struct tt*, unsigned log2_size);

struct tt* tt_resize_mb(struct tt*, unsigned megabytes);

void tt_destroy(struct tt*);

struct tt_entry tt_lookup(const struct tt*, const struct position*)
	attribute(nonnull);

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH

void tt_prefetch(const struct tt*, uint64_t key) attribute(nonnull);

#else

static inline void tt_prefetch(const struct tt *table, uint64_t key)
{
	(void) tt;
	(void) key;
}

#endif

size_t tt_slot_count(const struct tt*)
	attribute(nonnull);

void tt_pos_insert(struct tt*, const struct position*, struct tt_entry)
	attribute(nonnull);

void tt_extract_pv(const struct tt*, const struct position*,
			int depth, struct move pv[], int value)
	attribute(nonnull);

size_t tt_size(const struct tt*)
	attribute(nonnull);

bool tt_is_mb_size_valid(unsigned megabytes);

unsigned tt_min_size_mb(void);

unsigned tt_max_size_mb(void);

void tt_clear(struct tt*)
	attribute(nonnull);

void tt_swap(struct tt*)
	attribute(nonnull);

size_t tt_usage(const struct tt*)
	attribute(nonnull);

uint64_t position_polyglot_key(const struct position*)
	attribute(nonnull);

void tt_generation_step(struct tt*)
	attribute(nonnull);

#endif
