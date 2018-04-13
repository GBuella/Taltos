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

#ifndef TALTOS_SEARCH_H
#define TALTOS_SEARCH_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "chess.h"
#include "position.h"
#include "tt.h"
#include "util.h"
#include "taltos.h"

#ifndef MAX_THREAD_COUNT
// single thread for now...
#define MAX_THREAD_COUNT 1
#endif

static_assert(MAX_THREAD_COUNT > 0, "invalid MAX_THREAD_COUNT");

struct search_description {
	int depth;
	int depth_limit;

	struct tt *tt;

	// todo: const struct tt *tt_paralell[MAX_THREAD_COUNT];

	struct position repeated_positions[26];

	uintmax_t time_limit;
	taltos_systime thinking_started;

	uintmax_t node_count_limit;

	struct search_settings settings;
};

struct search_result {
	bool is_terminated;
	int value;
	struct move best_move;
	unsigned selective_depth;
	unsigned qdepth;
	uintmax_t node_count;
	uintmax_t qnode_count;
	uintmax_t cutoff_count;
	uintmax_t first_move_cutoff_count;
	struct move pv[MAX_PLY];
};

void init_search(void);

struct search_result search(const struct position*,
				const struct search_description*,
				volatile bool *run_flag,
				const struct move *prev_pv)
	attribute(nonnull);

#endif
