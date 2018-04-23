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



struct tt *tt_create(unsigned log2_size);

struct tt *tt_create_mb(unsigned megabytes);

struct tt *tt_resize(struct tt*, unsigned log2_size);

struct tt* tt_resize_mb(struct tt*, unsigned megabytes);

void tt_destroy(struct tt*);

#ifdef TALTOS_CAN_USE_BUILTIN_PREFETCH

void tt_prefetch(const struct tt*, uint64_t key) attribute(nonnull);

#else

static inline void tt_prefetch(const struct tt *table, uint64_t key)
{
	(void) tt;
	(void) key;
}

#endif

size_t tt_slot_count(const struct tt*);

void tt_insert_exact_value(struct tt*, uint64_t key,
			   int16_t value, int depth, bool no_null_flag);

void tt_insert_lower_bound(struct tt*, uint64_t key,
			   int16_t value, int depth, bool no_null_flag);

void tt_insert_exact_valuem(struct tt*, uint64_t key, uint8_t mg_index,
			    int16_t value, int depth, bool no_null_flag);

void tt_insert_lower_boundm(struct tt*, uint64_t key, uint8_t mg_index,
			    int16_t value, int depth, bool no_null_flag);

void tt_insert_move(struct tt*, uint64_t key, uint8_t mg_index,
		    bool no_null_flag);

struct tt_lookup_result {
	int16_t value;
	uint8_t depth;
	uint8_t moves[3];
	uint8_t move_count;
	bool found_value;
	bool found_exact_value;
	bool no_null_flag;
};

void tt_lookup(struct tt*, uint64_t key, int depth,
	       struct tt_lookup_result*);

struct tt_value {
	int16_t value;
	bool is_exact_value;
};

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

enum { tt_max_depth = 255 };

#endif
