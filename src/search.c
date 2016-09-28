
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <threads.h>

#include "macros.h"
#include "search.h"
#include "position.h"
#include "hash.h"
#include "eval.h"
#include "move_order.h"
#include "util.h"

#define NODES_PER_CANCEL_TEST 10000

#define NON_VALUE INT_MIN

enum {
	node_array_length = MAX_PLY + MAX_Q_PLY + 32,
	nmr_factor = 2 * PLY
};

enum node_type {
	unknown = 0,
	PV_node,
	all_node,
	cut_node
};

struct nodes_common_data {
	struct search_result result;
	jmp_buf terminate_jmp_buf;
	volatile atomic_flag *run_flag;
	struct search_description sd;
};

struct node {
	struct position pos[1];
	struct hash_table *tt;
	unsigned root_distance;
	enum node_type expected_type;
	int lower_bound;
	int upper_bound;
	int alpha;
	int beta;
	int depth;
	int value;
	bool is_search_root;
	ht_entry deep_entry;
	ht_entry fresh_entry;
	move best_move;
	bool is_GHI_barrier;
	bool search_reached;
	bool any_search_reached;
	struct nodes_common_data *common;
	struct move_fsm move_fsm;
};

static bool
is_qsearch(const struct node *node)
{
	return node->depth <= 0 && !is_in_check(node->pos);
}

static void negamax(struct node*);

#ifndef max
static int
max(int a, int b)
{
	return (a > b) ? a : b;
}
#endif

static bool
is_repetition(const struct node *node)
{
#ifndef SEARCH_CHECK_REPETITIONS
	return false;
#endif

	// todo: find out if this is useful

	const uint64_t *zhash = node->pos->zhash;

	while (!node->is_GHI_barrier) {
		node -= 2;
		if (node->pos->zhash[0] == zhash[0]
		    && node->pos->zhash[1] == zhash[1])
			return true;
	}
	return false;
}

static ht_entry
hash_current_node_value(const struct node *node)
{
	ht_entry entry;

	entry = ht_set_depth(HT_NULL, node->depth);

	if (node->value <= node->lower_bound)
		entry = ht_set_value(entry, vt_exact, node->lower_bound);
	else if (node->best_move == 0)
		entry = ht_set_value(entry, vt_upper_bound, node->value);
	else if (node->value >= node->upper_bound)
		entry = ht_set_value(entry, vt_exact, node->upper_bound);
	else if (node->value >= node->beta)
		entry = ht_set_value(entry, vt_lower_bound, node->value);
	else
		entry = ht_set_value(entry, vt_exact, node->value);

	return entry;
}

static void
setup_best_move(struct node *node)
{
	if (node->best_move == 0) {
		if (ht_has_move(node->deep_entry))
			node->best_move = ht_move(node->deep_entry);
		else if (ht_has_move(node->fresh_entry))
			node->best_move = ht_move(node->fresh_entry);
	}
}

static void
handle_node_types(struct node *node)
{
	struct node *child = node + 1;

	switch (node->expected_type) {
	case PV_node:
		if (node->move_fsm.index == 0)
			child->expected_type = PV_node;
		else
			child->expected_type = cut_node;
		break;
	case all_node:
		child->expected_type = cut_node;
		break;
	case cut_node:
		if (node->move_fsm.index > 0
		    && node->move_fsm.is_in_late_move_phase) {
			node->expected_type = all_node;
			child->expected_type = cut_node;
		}
		else {
			child->expected_type = all_node;
		}
		break;
	default:
		unreachable;
	}
}

static int
get_lmr_factor(const struct node *node)
{
	if (!node->common->sd.settings.use_lmr)
		return 0;

	int reduction;

	if ((node->root_distance == 0)
	    || is_in_check(node[0].pos)
	    || is_in_check(node[1].pos)
	    || (node->depth <= PLY * 2)
	    || !node->move_fsm.is_in_late_move_phase
	    || node->move_fsm.count < 16
	    || (node->move_fsm.index < 2))
		return 0;

	if (node->depth > 6 * PLY)
		reduction = node->depth / 4;
	else
		reduction = PLY + PLY / 2;

	if (node->move_fsm.index > node->move_fsm.very_late_moves_begin)
		reduction += PLY / 2;

	if (node->depth - reduction < PLY * 2)
		reduction = node->depth - PLY * 2;

	return reduction;
}

static void
fail_high(struct node *node)
{
	if (node->depth > 0 && !move_fsm_done(&node->move_fsm)) {
		node->common->result.cutoff_count++;
		if (node->move_fsm.index == 1)
			node->common->result.first_move_cutoff_count++;
	}
	if (node->move_fsm.is_in_late_move_phase)
		move_fsm_add_killer(&node->move_fsm, node->best_move);
}



/*
 * negamax helper functions
 */


static void
house_keeping(struct node *node)
{
	node->common->result.node_count++;

	if (node->common->result.node_count
	    == node->common->sd.node_count_limit)
		longjmp(node->common->terminate_jmp_buf, 1);
	if (node->common->result.node_count % NODES_PER_CANCEL_TEST == 0) {
		if (node->common->sd.time_limit != 0) {
			unsigned long time =
			    xseconds_since(node->common->sd.thinking_started);
			if (time >= node->common->sd.time_limit)
				longjmp(node->common->terminate_jmp_buf, 1);
		}
		if (!atomic_flag_test_and_set(node->common->run_flag))
			longjmp(node->common->terminate_jmp_buf, 1);
	}

	node->any_search_reached = true;
	if (!is_qsearch(node))
		node->search_reached = true;
}

enum { prune_successfull = 1 };

static int
try_null_move_prune(struct node *node)
{
	// disabled for now
	(void) node;
	return 0;
	if (!   (node->expected_type == cut_node)
	    && (node->depth > nmr_factor + PLY)
	    && !is_in_check(node->pos)
	    && (node->move_fsm.count > 16)
	    && (popcnt(node->pos->map[1]) > 6)
	    && (node->pos->material_value > node->beta))
		return 0;

	struct node *child_node = node + 1;

	position_flip(child_node->pos, node->pos);
	child_node->expected_type = all_node;
	child_node->depth = node->depth - nmr_factor;
	child_node->alpha = -node->beta - 1;
	child_node->beta = -node->beta;
	negamax(child_node);
	if (-child_node->value >= node->beta) {
		node->value = -child_node->value;
		return prune_successfull;
	}
	return 0;
}

enum { cutoff = 1 };

static int
check_hash_value(struct node *node, ht_entry entry)
{
	if (node->depth > ht_depth(entry)) {
		if (ht_value_type(entry) == vt_lower_bound
		    || ht_value_type(entry) == vt_exact) {
			if (ht_value(entry) >= mate_value) {
				node->value = ht_value(entry);
				node->best_move = ht_move(entry);
				return cutoff;
			}
		}
		if (ht_value_type(entry) == vt_upper_bound
		    || ht_value_type(entry) == vt_exact) {
			if (ht_value(entry) <= -mate_value) {
				node->value = ht_value(entry);
				if (node->best_move == 0)
					node->best_move = ht_move(entry);
				return cutoff;
			}
		}
		return 0;
	}

	switch (ht_value_type(entry)) {
	case vt_lower_bound:
		if (node->lower_bound < ht_value(entry)) {
			node->lower_bound = ht_value(entry);
			if (node->lower_bound >= node->beta) {
				node->best_move = ht_move(entry);
				node->value = node->lower_bound;
				return cutoff;
			}
		}
		break;
	case vt_upper_bound:
		if (node->upper_bound > ht_value(entry)) {
			node->upper_bound = ht_value(entry);
			if (node->upper_bound <= node->alpha) {
				node->value = node->upper_bound;
				return cutoff;
			}
		}
		break;
	case vt_exact:
		node->value = ht_value(entry);
		node->best_move = ht_move(entry);
		return cutoff;
	default:
		break;
	}
	if (node->lower_bound >= node->upper_bound) {
		node->value = node->lower_bound;
		return cutoff;
	}
	return 0;
}

enum { too_deep = 1 };

static int
adjust_depth(struct node *node)
{
	if (node->root_distance == 0)
		return 0;

	if (node->root_distance >= node_array_length - 1) {
		// search space exploding exponentially?
		// forced static eval, cut tree at this depth
		node->value = eval(node->pos);
		return too_deep;
	}
	if (is_in_check(node->pos))
		node->depth++;
	else if (node->depth < PLY && node->depth > 0)
		node->depth = PLY;
	return 0;
}

static int
fetch_hash_value(struct node *node)
{
	node->deep_entry = 0;
	node->fresh_entry = 0;

	ht_entry entry = ht_lookup_deep(node->tt, node->pos,
	    node->depth, node->beta);
	if (ht_is_set(entry)
	    && (move_fsm_add_hash_move(&node->move_fsm, ht_move(entry)) == 0)) {
		node->deep_entry = entry;
		if (check_hash_value(node, entry) == cutoff)
			return cutoff;
	}
	entry = ht_lookup_fresh(node->tt, node->pos);
	if (!ht_is_set(entry))
		return 0;
	if (node->move_fsm.has_hashmove) {
		size_t i = 0;
		while (i < node->move_fsm.count) {
			if (node->move_fsm.moves[i++] == ht_move(entry))
				break;
		}
		if (i == node->move_fsm.count)
			return 0;
	}
	else if (move_fsm_add_hash_move(&node->move_fsm, ht_move(entry)) != 0)
		return 0;
	node->fresh_entry = entry;
	if (check_hash_value(node, entry) == cutoff) {
		ht_pos_insert(node->tt, node->pos, entry);
		return cutoff;
	}

	return 0;
}

enum { no_legal_moves = 1 };

static int
setup_moves(struct node *node)
{
	move_fsm_setup(&node->move_fsm, node->pos, is_qsearch(node));
	if (node->move_fsm.count == 0) {
		if (is_qsearch(node))
			node->value = node->lower_bound;  // leaf node
		else if (is_in_check(node->pos))
			node->value = -max_value;  // checkmate
		else {
			node->value = 0;   // stalemate
		}
		return no_legal_moves;
	}
	return 0;
}

static int
mate_adjust(int value)
{
	if (value > mate_value)
		return value - 1;
	else
		return value;
}

static bool
search_more_moves(const struct node *node)
{
	if (move_fsm_done(&node->move_fsm))
		return false; // no more moves

	/*
	 * if depth < 0 then search only tactical moves,
	 * except when in check.
	 * A special case is when depth < 0 and in check.
	 * All legal moves are generated, and at least one
	 * must be searched, or enough to prove that it is not
	 * a position leading to checkmate in qsearch.
	 */
	if (node->depth >= 0)
		return true;
	else // if in_check
		return node->value <= -mate_value
		    || !node->move_fsm.is_in_late_move_phase;
}

static void
negamax(struct node *node)
{
	assert(node->alpha < node->beta);

	house_keeping(node);
	node->value = NON_VALUE;
	node->best_move = 0;
	if (has_insufficient_material(node->pos) || is_repetition(node)) {
		node->value = 0;
		return;
	}
	ht_prefetch(node->tt, pos_hash(node->pos));
	if (adjust_depth(node) == too_deep)
		return;
	node->upper_bound = max_value;
	if (is_qsearch(node)) {
		node->lower_bound = eval(node->pos);
		if (node->lower_bound >= node->beta) {
			node->value = node->lower_bound;
			return;
		}
	}
	else {
		node->lower_bound = -max_value;
	}
	if (setup_moves(node) == no_legal_moves)
		return;
	if (node->root_distance > 0) {
		if (is_singular(node->pos->map[1]))
			node->lower_bound = max(0, node->lower_bound);
		if (is_singular(node->pos->map[0]))
			node->upper_bound = 0;
	}
	if (fetch_hash_value(node) == cutoff) {
		return;
	}
	if (node->lower_bound > node->alpha)
		node->alpha = node->lower_bound;
	if (node->upper_bound < node->beta)
		node->beta = node->upper_bound;

	if (node->alpha >= node->beta) {
		node->value = node->lower_bound;
		return;
	}
	assert(node->alpha < node->beta);

	if (false && try_null_move_prune(node) == prune_successfull)
		return;


	do {
		handle_node_types(node);
		move m = select_next_move(node->pos, &node->move_fsm);
		make_move(node[1].pos, node->pos, m);
		node[1].is_GHI_barrier = is_move_irreversible(node->pos, m);
		int lmr_factor = get_lmr_factor(node);

		node[1].depth = node->depth - PLY - lmr_factor;
		node[1].beta = -node->alpha;
		node[1].alpha = (lmr_factor ? (-node->alpha - 1) : -node->beta);
		negamax(node + 1);
		if (-node[1].value > node->value && lmr_factor != 0) {
			node[1].depth = node->depth - PLY;
			node[1].alpha = -node->beta;
			node[1].beta = -node->alpha;
			negamax(node + 1);
		}
		if (-node[1].value > node->value) {
			node->value = mate_adjust(-node[1].value);
			if (node->value > node->alpha) {
				node->alpha = node->value;
				node->best_move = m;
				if (node->alpha >= node->beta) {
					fail_high(node);
					break;
				}
				node->expected_type = PV_node;
			}
		}
	} while (search_more_moves(node));

	ht_entry entry = hash_current_node_value(node);
	setup_best_move(node);
	if (node->best_move != 0)
		entry = ht_set_move(entry, node->best_move);
	ht_pos_insert(node->tt, node->pos, entry);
	if (node->value < node->lower_bound)
		node->value = node->lower_bound;
}



static void
setup_node_array(size_t count, struct node nodes[count],
		struct search_description sd,
		struct nodes_common_data *common)
{
	for (unsigned i = 1; i < count; ++i) {
		nodes[i].root_distance = i - 1;
		nodes[i].tt = sd.tt;
		nodes[i].common = common;
	}
	nodes[count - 1].root_distance = 0xffff;
}

static struct node*
setup_root_node(struct node *nodes, const struct position *pos)
{
	nodes[0].search_reached = true;
	nodes[1].search_reached = true;
	nodes[0].any_search_reached = true;
	nodes[1].any_search_reached = true;
	nodes[0].is_GHI_barrier = true;
	nodes[1].is_GHI_barrier = true;
	nodes[0].is_search_root = true;
	nodes[1].alpha = -max_value;
	nodes[1].beta = max_value;
	nodes[1].depth = nodes[1].common->sd.depth;
	nodes[1].is_search_root = true;
	nodes[1].expected_type = PV_node;
	nodes[1].pos[0] = *pos;

	return nodes + 1;
}

static unsigned
find_selective_depth(const struct node nodes[])
{
	unsigned depth = 1;
	while (nodes[depth].search_reached)
		++depth;

	return (depth - 2);
}

static unsigned
find_qdepth(const struct node nodes[])
{
	unsigned depth = 1;
	while (nodes[depth].any_search_reached)
		++depth;

	return depth - 2;
}

struct search_result
search(const struct position *root_pos,
	struct search_description sd,
	volatile atomic_flag *run_flag)
{
	struct node *nodes;
	struct nodes_common_data common;
	struct node *root_node;

	nodes = xaligned_calloc(pos_alignment,
	    sizeof(nodes[0]), node_array_length);
	memset(&common, 0, sizeof common);
	common.run_flag = run_flag;
	common.sd = sd;
	setup_node_array(node_array_length, nodes, sd, &common);
	root_node = setup_root_node(nodes, root_pos);

	if (setjmp(common.terminate_jmp_buf) == 0) {
		setup_registers();
		negamax(root_node);
		common.result.value = root_node->value;
		common.result.best_move = root_node->best_move;
		common.result.selective_depth = find_selective_depth(nodes);
		common.result.qdepth = find_qdepth(nodes);
	}
	else {
		common.result.is_terminated = true;
	}

	xaligned_free(nodes);
	return common.result;
}
