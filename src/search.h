
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_SEARCH_H
#define TALTOS_SEARCH_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#include "chess.h"
#include "position.h"
#include "hash.h"
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

	struct hash_table *tt;

	// todo: const struct hash_table *tt_paralell[MAX_THREAD_COUNT];

	struct position repeated_positions[26];

	uintmax_t time_limit;
	taltos_systime thinking_started;

	uintmax_t node_count_limit;

	struct search_settings settings;
};

struct search_result {
	bool is_terminated;
	int value;
	move best_move;
	unsigned selective_depth;
	unsigned qdepth;
	uintmax_t node_count;
	uintmax_t qnode_count;
	uintmax_t cutoff_count;
	uintmax_t first_move_cutoff_count;
	move pv[MAX_PLY];
};


struct search_result search(const struct position*,
				enum player debug_player_to_move,
				struct search_description,
				volatile atomic_flag *run_flag)
	attribute(nonnull);

#endif
