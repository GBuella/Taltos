
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <threads.h>
#include <math.h>

#include "macros.h"
#include "search.h"
#include "position.h"
#include "hash.h"
#include "eval.h"
#include "move_order.h"
#include "util.h"

#define NODES_PER_CANCEL_TEST 10000

#define NON_VALUE INT_MIN

enum { node_array_length = MAX_PLY + MAX_Q_PLY + 32 };

enum node_type {
	PV_node,
	all_node,
	cut_node
};

struct nodes_common_data {
	struct search_result result;
	jmp_buf terminate_jmp_buf;
	volatile atomic_flag *run_flag;
	struct search_description sd;

	enum player debug_root_player_to_move;
	char debug_move_stack[0x10000];
	char *debug_move_stack_end;
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
	bool has_repetition_in_history;
	bool search_reached;
	bool any_search_reached;
	struct nodes_common_data *common;
	struct move_order mo[1];
	unsigned non_pawn_move_count;
	unsigned non_pawn_move_piece_count;

	int static_value;
	bool static_value_computed;

	uint64_t opp_king_sliding_reach;
	uint64_t opp_king_knight_reach;
	uint64_t opp_king_pawn_reach;
	bool king_reach_maps_computed;

	enum player debug_player_to_move;

	move pv[MAX_PLY];
	move forced_pv;
	move forced_extension;

	int repetition_affected_best;
	int repetition_affected_any;

	int LMR_subject_index;

	bool is_in_null_move_search;
	bool null_move_search_failed;
};

static void
debug_trace_tree_init(struct node *node)
{
#ifdef NDEBUG
	(void) node;
#else
	if (node->root_distance == 0) {
		node->debug_player_to_move =
		    node->common->debug_root_player_to_move;
		node->common->debug_move_stack_end =
		    node->common->debug_move_stack;
		node->common->debug_move_stack[0] = '\0';
	}
	else {
		node->debug_player_to_move =
		    opponent_of(node[-1].debug_player_to_move);
	}
#endif
}

static void
debug_trace_tree_push_move(struct node *node, move m)
{
#ifdef NDEBUG
	(void) node;
	(void) m;
#else
	assert(node->common->debug_move_stack_end[0] == '\0');

	*(node->common->debug_move_stack_end++) = ' ';

	if (m != 0) {
		node->common->debug_move_stack_end = print_coor_move(
		    m, node->common->debug_move_stack_end,
		    node->debug_player_to_move);
	}
	else {
		strcpy(node->common->debug_move_stack_end, "null");
		node->common->debug_move_stack_end += 4;
	}

#endif
}

static void
debug_trace_tree_pop_move(struct node *node)
{
#ifdef NDEBUG
	(void) node;
#else
	assert(node->common->debug_move_stack_end
	    > node->common->debug_move_stack);

	assert(node->common->debug_move_stack_end[0] == '\0');

	while (*(node->common->debug_move_stack_end) != ' ')
		node->common->debug_move_stack_end--;

	node->common->debug_move_stack_end[0] = '\0';
#endif
}

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

static ht_entry
hash_current_node_value(const struct node *node)
{
	ht_entry entry;

	entry = ht_set_depth(HT_NULL, node->depth);

	if (node->best_move == 0) {
		if (node->alpha < 0 || node->repetition_affected_any == 0)
			entry = ht_set_value(entry,
			    vt_upper_bound, node->alpha);
	}
	else if (node->value >= node->beta) {
		if (node->beta > 0 || node->repetition_affected_best == 0)
			entry = ht_set_value(entry, vt_lower_bound, node->beta);
	}
	else {
		int type = 0;
		if (node->value < 0 || node->repetition_affected_any == 0)
			type |= vt_upper_bound;
		if (node->value > 0 || node->repetition_affected_best == 0)
			type |= vt_lower_bound;

		if (type != 0)
			entry = ht_set_value(entry, type, node->value);
	}

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
		if (node->mo->index == 0)
			child->expected_type = PV_node;
		else
			child->expected_type = cut_node;
		break;
	case all_node:
		child->expected_type = cut_node;
		break;
	case cut_node:
		if (node->mo->index > 0) {
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
get_static_value(struct node *node)
{
	if (!node->static_value_computed) {
		node->static_value = eval(node->pos);
		node->static_value_computed = true;
	}

	return node->static_value;
}

static int LMR[20 * PLY][64];
static unsigned LMP[3 * PLY + 1] = { 0, 6, 6, 10, 12, 16, 20};

static int
get_LMR_factor(struct node *node)
{
	if (!node->common->sd.settings.use_LMR)
		return 0;

	if (node->LMR_subject_index == -1) {
		if (node->mo->index >= 1
		    && mo_current_move_value(node->mo) < 0) {
			node->LMR_subject_index = 0;
		}
		else {
			return 0;
		}
	}
	else {
		node->LMR_subject_index++;
	}

	if (is_in_check(node[0].pos)
	    || is_in_check(node[1].pos)
	    || (node->depth <= PLY)
	    || (node->non_pawn_move_count < 7 && node->mo->count < 10))
		return 0;

	int d = node->depth;
	if (d >= (int)ARRAY_LENGTH(LMR))
		d = (int)ARRAY_LENGTH(LMR);

	int index = node->LMR_subject_index;
	if (index >= (int)ARRAY_LENGTH(LMR[node->depth]))
		index = (int)ARRAY_LENGTH(LMR[node->depth]) - 1;

	int r = LMR[d][index];
	if (node->expected_type == PV_node && r > 0)
		--r;
	else if (mo_current_move_value(node->mo) < - 1000)
		++r;

	return r;
}

static void
fail_high(struct node *node)
{
	if (node->depth > 0 && !move_order_done(node->mo)) {
		node->common->result.cutoff_count++;
		if (node->mo->index == 0)
			node->common->result.first_move_cutoff_count++;
	}
	if (!mo_current_move_is_tactical(node->mo))
		move_order_add_killer(node->mo, node->best_move);

	if (node->depth > 2 * PLY)
		move_order_adjust_history_on_cutoff(node->mo);
}



/*
 * negamax helper functions
 */

static void
check_node_count_limit(struct nodes_common_data *data)
{
	if (data->result.node_count == data->sd.node_count_limit)
		longjmp(data->terminate_jmp_buf, 1);
}

static void
check_time_limit(struct nodes_common_data *data)
{
	if (data->sd.time_limit == 0)
		return; // zero means no limit

	if (xseconds_since(data->sd.thinking_started) >= data->sd.time_limit)
		longjmp(data->terminate_jmp_buf, 1);
}

static void
check_run_flag(struct nodes_common_data *data)
{
	if (!atomic_flag_test_and_set(data->run_flag))
		longjmp(data->terminate_jmp_buf, 1);
}

static void
node_init(struct node *node)
{
	node->common->result.node_count++;
	if (is_qsearch(node))
		node->common->result.qnode_count++;

	debug_trace_tree_init(node);

	node->is_in_null_move_search = false;
	node->null_move_search_failed = false;
	node->repetition_affected_any = 0;
	node->repetition_affected_best = 0;
	node->non_pawn_move_count = 0;
	node->non_pawn_move_piece_count = 0;
	node->LMR_subject_index = -1;

	node->pv[0] = 0;

	check_node_count_limit(node->common);

	if (node->common->result.node_count % NODES_PER_CANCEL_TEST == 0) {
		check_time_limit(node->common);
		check_run_flag(node->common);
	}

	node->any_search_reached = true;
	if (!is_qsearch(node))
		node->search_reached = true;

	node->value = NON_VALUE;
	node->best_move = 0;
	node->static_value_computed = false;
	node->king_reach_maps_computed = false;

	node->deep_entry = 0;
	node->fresh_entry = 0;
}

static int
negamax_child(struct node *node)
{
	negamax(node + 1);

	if (node->forced_pv == 0)
		node[1].forced_pv = 0;

	int value = -node[1].value;

	if (node->common->sd.settings.use_strict_repetition_check) {
		node->repetition_affected_any = max(
		    node->repetition_affected_any,
		    node[1].repetition_affected_best - 1);
	}

	/*
	 * Decrement a mate value each time it is passed down towards root,
	 * so represent that "mate in 5" is better than "mate in 6" --> the
	 * closer to the root the checkmate is, the better is is.
	 */
	if (value > mate_value)
		return value - 1;
	else
		return value;
}

enum { prune_successfull = 1 };

static bool
ht_entry_prevents_null_move(struct node *node, ht_entry entry)
{
	if (!ht_is_set(entry) || ht_depth(entry) < node->depth - 2 * PLY)
		return false;

	if (ht_no_null(entry))
		return true;

	if (ht_value_is_upper_bound(entry)) {
		if (ht_value(entry) <= node->beta)
			return true;
	}

	return false;
}

static bool
opponent_has_positive_capture(const struct node *node)
{
	const struct position *pos = node->pos;

	uint64_t attacks = pos->attack[opponent_pawn];
	uint64_t victims = pos->map[0] & ~pos->map[pawn];

	if (is_nonempty(attacks & victims))
		return true;

	attacks |= pos->attack[opponent_knight];
	attacks |= pos->attack[opponent_bishop];
	victims &= ~pos->map[bishop];
	victims &= ~pos->map[knight];

	if (is_nonempty(attacks & victims))
		return true;

	attacks |= pos->attack[opponent_rook];
	victims = pos->map[queen];

	if (is_nonempty(attacks & victims))
		return true;

	attacks = pos->attack[opponent_king];
	victims = pos->map[0] & ~pos->attack[0];

	if (is_nonempty(attacks & victims))
		return true;

	return false;
}

static int
try_null_move_prune(struct node *node)
{
	if (!node->common->sd.settings.use_null_moves)
		return 0;

	if (node->forced_pv != 0)
		return 0;

	if (ht_entry_prevents_null_move(node, node->deep_entry))
		return 0;

	if (ht_entry_prevents_null_move(node, node->fresh_entry))
		return 0;

	if (node->has_repetition_in_history)
		return 0;

	int advantage = get_static_value(node) - node->beta;

	if ((node->root_distance == 0)
	    || node[-1].is_in_null_move_search
	    || (node->expected_type == PV_node)
	    || (advantage < - 100)
	    || (node->depth <= 2 * PLY)
	    || is_in_check(node->pos)
	    || (node->mo->count < 18))
		return 0;

	if (advantage < 0 && opponent_has_positive_capture(node))
		return 0;

	if (node->non_pawn_move_count < 9
	    || node->non_pawn_move_piece_count < 2)
		return 0;

	struct node *child = node + 1;

	position_flip(child->pos, node->pos);

	child->has_repetition_in_history = false;
	child->is_GHI_barrier = true;
	child->expected_type = all_node;
	child->alpha = -node->beta - pawn_value - 1;
	child->beta = -node->beta - pawn_value;

	child->depth = node->depth - PLY - 3 * PLY;

	node->is_in_null_move_search = true;
	debug_trace_tree_push_move(node, 0);
	int value = negamax_child(node);
	debug_trace_tree_pop_move(node);
	node->is_in_null_move_search = false;

	if (value > node->beta + pawn_value && value <= mate_value) {
		node->value = node->beta;
		return prune_successfull;
	}
	else {
		node->null_move_search_failed = true;
	}

	return 0;
}

enum { hash_cutoff = 1 };

static int
check_hash_value_lower_bound(struct node *node, ht_entry entry)
{
	int hash_bound = ht_value(entry);

	if (hash_bound > 0 && node->has_repetition_in_history)
		hash_bound = 0;

	if (node->lower_bound < hash_bound) {
		node->lower_bound = hash_bound;
		if (node->lower_bound >= node->beta) {
			node->best_move = ht_move(entry);
			node->value = node->lower_bound;
			return hash_cutoff;
		}
	}
	return 0;
}

static int
check_hash_value_upper_bound(struct node *node, ht_entry entry)
{
	int hash_bound = ht_value(entry);

	if (hash_bound < 0 && node->has_repetition_in_history)
		hash_bound = 0;

	if (node->upper_bound > hash_bound) {
		node->upper_bound = hash_bound;
		if (node->upper_bound <= node->alpha) {
			node->value = node->upper_bound;
			return hash_cutoff;
		}
	}
	return 0;
}

static int
check_hash_value(struct node *node, ht_entry entry)
{
	if (node->root_distance == 0)
		return 0;

	if (node[-1].forced_pv != 0
	    && node->alpha == -max_value
	    && node->beta == max_value)
		return 0;

	if (node->depth > ht_depth(entry)) {
		if (ht_value_is_lower_bound(entry)) {
			if (ht_value(entry) >= mate_value) {
				if (!node->has_repetition_in_history) {
					node->value = ht_value(entry);
					node->best_move = ht_move(entry);
					return hash_cutoff;
				}
				else if (node->lower_bound < 0) {
					node->lower_bound = 0;
					if (0 >= node->beta) {
						node->value = node->beta;
						return hash_cutoff;
					}
				}
			}
		}
		if (ht_value_is_upper_bound(entry)) {
			if (ht_value(entry) <= -mate_value) {
				if (!node->has_repetition_in_history) {
					node->value = ht_value(entry);
					return hash_cutoff;
				}
				else if (node->upper_bound > 0) {
					node->upper_bound = 0;
					if (node->alpha >= 0) {
						node->value = node->alpha;
						return hash_cutoff;
					}
				}
			}
		}
		return 0;
	}

	if (ht_value_is_lower_bound(entry))
		if (check_hash_value_lower_bound(node, entry) == hash_cutoff)
			return hash_cutoff;

	if (ht_value_is_upper_bound(entry))
		if (check_hash_value_upper_bound(node, entry) == hash_cutoff)
			return hash_cutoff;

	if (node->lower_bound >= node->upper_bound) {
		node->value = node->lower_bound;
		return hash_cutoff;
	}

	return 0;
}

enum { too_deep = 1 };

static int
adjust_depth(struct node *node)
{
	// Don't touch the depth of the root node!
	if (node->root_distance == 0)
		return 0;

	node->depth += node->forced_extension;

	if (node->root_distance >= node_array_length - 1) {
		// search space exploding exponentially?
		// forced static eval, cut tree at this depth
		node->value = get_static_value(node);
		return too_deep;
	}

	if (node->depth < PLY && node->depth > 0)
		node->depth = PLY;

	return 0;
}

static int
fetch_hash_value(struct node *node)
{
	ht_entry entry = ht_lookup_deep(node->tt, node->pos,
	    node->depth, node->beta);
	if (ht_is_set(entry)) {
		if (move_order_add_hint(node->mo, ht_move(entry), 1) == 0) {
			node->deep_entry = entry;
			if (check_hash_value(node, entry) == hash_cutoff)
				return hash_cutoff;
		}
	}

	entry = ht_lookup_fresh(node->tt, node->pos);
	if (!ht_is_set(entry))
		return 0;

	if (ht_has_move(node->deep_entry)) {
		if (move_order_add_weak_hint(node->mo, ht_move(entry)) != 0)
			return 0;
	}
	else {
		if (move_order_add_hint(node->mo, ht_move(entry), 1) != 0)
			return 0;
	}

	node->fresh_entry = entry;
	if (check_hash_value(node, entry) == hash_cutoff) {
		ht_pos_insert(node->tt, node->pos, entry);
		return hash_cutoff;
	}

	return 0;
}

enum { no_legal_moves = 1 };

static int
setup_moves(struct node *node)
{
	bool am = node->common->sd.settings.use_advanced_move_order;
	am = am && node->depth > 2 * PLY;

	move_order_setup(node->mo, node->pos,
	    is_qsearch(node), am, get_static_value(node),
	    node->root_distance % 2);

	if (node->mo->count == 0) {
		if (is_qsearch(node))
			node->value = node->lower_bound;  // leaf node
		else if (is_in_check(node->pos)) {
			node->value = -max_value;  // checkmate
		}
		else {
			node->value = 0;   // stalemate
		}
		return no_legal_moves;
	}

	bool moving_piece[PIECE_ARRAY_SIZE] = {0};

	for (unsigned i = 0; i < node->mo->count; ++i) {
		enum piece p = mresultp(node->mo->moves[i]);
		if (p != pawn) {
			node->non_pawn_move_count++;
			if (!moving_piece[p]) {
				node->non_pawn_move_piece_count++;
				moving_piece[p] = true;
			}
		}
	}

	return 0;
}

static bool
search_more_moves(const struct node *node)
{
	if (move_order_done(node->mo))
		return false; // no more moves

	if (node->alpha >= node->beta)
		return false;

	if (node->root_distance > 3
	    && node->value > - mate_value
	    && !is_in_check(node->pos)
	    && node->depth > 0 && node->depth < (int)ARRAY_LENGTH(LMP)) {
		if (mo_current_move_value(node->mo) <= 0) {
			if (node->mo->index >= LMP[node->depth])
				return false;
		}
	}

	return true;
}

enum { stand_pat_cutoff = 1 };

static int
setup_bounds(struct node *node)
{
	node->upper_bound = max_value;

	if (is_qsearch(node)) {
		node->lower_bound = get_static_value(node);
		if (node->lower_bound >= node->beta) {
			node->value = node->lower_bound;
			return stand_pat_cutoff;
		}
	}
	else {
		node->lower_bound = -max_value;
	}

	// if a side has only a king left, the best value it can get is zero
	if (node->root_distance > 0) {
		if (is_singular(node->pos->map[1]))
			node->lower_bound = max(0, node->lower_bound);

		// no need to use "min(0, node->upper_bound)",
		// upper_bound is not affected before this
		if (is_singular(node->pos->map[0]))
			node->upper_bound = 0;
	}

	return 0;
}

enum { alpha_not_less_than_beta = 1 };

static int
recheck_bounds(struct node *node)
{
	if (node->lower_bound > node->alpha)
		node->alpha = node->lower_bound;

	if (node->upper_bound < node->beta)
		node->beta = node->upper_bound;

	if (node->alpha >= node->beta) {
		node->value = node->lower_bound;
		return alpha_not_less_than_beta;
	}

	return 0;
}

static int
find_repetition(struct node *node)
{
	if (!node->common->sd.settings.use_repetition_check)
		return 0;

	int r = 0;
	int found_first = 0;
	const uint64_t *zhash = node->pos->zhash;
	bool strict = node->common->sd.settings.use_strict_repetition_check;

	while (!node[-r].is_GHI_barrier) {
		r += 2;
		if (node[-r].pos->zhash[0] == zhash[0]
		    && node[-r].pos->zhash[1] == zhash[1]) {
			if (strict) {
				node->has_repetition_in_history = true;
				if (found_first != 0)
					return found_first;
				else
					found_first = r;
			}
			else {
				return r;
			}
		}
	}

	return 0;
}

enum { is_trivial_draw = 1 };

static int
check_trivial_draw(struct node *node)
{
	if (has_insufficient_material(node->pos)) {
		node->value = 0;
		return is_trivial_draw;
	}

	int r = find_repetition(node);
	if (r > 0) {
		node->repetition_affected_any = r;
		node->repetition_affected_best = r;
		node->value = 0;
		return is_trivial_draw;
	}

	return 0;
}

static void
setup_child_node(struct node *node, move m, int *LMR_factor)
{
	struct node *child = node + 1;

	if (node->common->sd.settings.use_repetition_check) {
		if (is_move_irreversible(node->pos, m)) {
			child->is_GHI_barrier = true;
			child->has_repetition_in_history = false;
		}
		else {
			child->is_GHI_barrier = false;
			child->has_repetition_in_history =
			    node->has_repetition_in_history;
		}
	}

	make_move(child->pos, node->pos, m);
	debug_trace_tree_push_move(node, m);
	handle_node_types(node);

	child->depth = node->depth - PLY;
	child->beta = -node->alpha;
	child->alpha = -node->beta;

	*LMR_factor = get_LMR_factor(node);

	if (*LMR_factor != 0) {
		child->depth -= *LMR_factor;
		child->alpha = -node->alpha - 1;
	}
}

static void
reset_child_after_lmr(struct node *node)
{
	node[1].depth = node->depth - PLY;
	node[1].alpha = -node->beta;
	node[1].beta = -node->alpha;
}

static void
new_best_move(struct node *node, move best)
{
	node->best_move = best;

	if (node->common->sd.settings.use_strict_repetition_check) {
		node->repetition_affected_best = max(
		    node->repetition_affected_best,
		    node[1].repetition_affected_any - 1);
	}

	if (node->alpha < node->beta) {
		node->pv[0] = best;
		unsigned i = 0;
		do {
			node->pv[i + 1] = node[1].pv[i];
			++i;
		} while (node->pv[i] != 0);
	}
}

static int
handle_beta_extension(struct node *node, move m, int value)
{
	if (!node->common->sd.settings.use_beta_extensions)
		return value;

	if (value >= node->beta
	    && !node[-1].is_in_null_move_search
	    && value < mate_value
	    && node->depth > PLY
	    && !is_capture(m)
	    && !is_promotion(m)
	    && mtype(m) != mt_castle_kingside
	    && mtype(m) != mt_castle_queenside
	    && mresultp(m) != pawn) {
		node[1].alpha = -node->beta;
		node[1].beta = -node->alpha;
		if (move_gives_check(m))
			node[1].depth = node->depth;
		else
			node[1].depth = node->depth - PLY / 2;
		return  negamax_child(node);
	}
	else {
		return value;
	}
}

static void
negamax(struct node *node)
{
	assert(node->alpha < node->beta);

	node_init(node);

	if (check_trivial_draw(node) == is_trivial_draw)
		return;

	ht_prefetch(node->tt, pos_hash(node->pos));

	if (adjust_depth(node) == too_deep)
		return;

	if (setup_bounds(node) == stand_pat_cutoff)
		return;

	if (setup_moves(node) == no_legal_moves)
		return;

	if (node->forced_pv == 0) {
		if (fetch_hash_value(node) == hash_cutoff)
			return;
	}
	else {
		int r = move_order_add_hint(node->mo, node->forced_pv, 0);
		(void) r;
		assert(r == 0);
	}

	if (try_null_move_prune(node) == prune_successfull)
		return;

	if (recheck_bounds(node) == alpha_not_less_than_beta)
		return;

	do {
		move_order_pick_next(node->mo);

		move m = mo_current_move(node->mo);

		int LMR_factor;

		setup_child_node(node, m, &LMR_factor);

		int value = negamax_child(node);

		if (LMR_factor != 0) {
			if (value > node->alpha) {
				reset_child_after_lmr(node);
				value = negamax_child(node);
			}
			else {
				debug_trace_tree_pop_move(node);
				continue;
			}
		}

		value = handle_beta_extension(node, m, value);

		if (value > node->value) {
			node->value = value;
			if (value > node->alpha) {
				node->alpha = value;
				new_best_move(node, m);

				if (node->alpha >= node->beta)
					fail_high(node);
				else
					node->expected_type = PV_node;
			}
		}
		debug_trace_tree_pop_move(node);
	} while (search_more_moves(node));

	ht_entry entry = hash_current_node_value(node);
	setup_best_move(node);

	assert(node->repetition_affected_any == 0 || !node->is_GHI_barrier);
	assert(node->repetition_affected_best == 0 || !node->is_GHI_barrier);

	if (node->depth > 0) {
		if (node->best_move != 0)
			entry = ht_set_move(entry, node->best_move);
		if (node->null_move_search_failed)
			entry = ht_set_no_null(entry);
		if (ht_value_type(entry) != 0 || node->best_move != 0)
			ht_pos_insert(node->tt, node->pos, entry);
	}
	if (node->value < node->lower_bound)
		node->value = node->lower_bound;
	node->forced_pv = 0;
}



static void
setup_node_array(size_t count, struct node nodes[count],
		struct search_description sd,
		struct nodes_common_data *common,
		const move *prev_pv)
{
	for (unsigned i = 1; i < count; ++i) {
		nodes[i].root_distance = i - 1;
		nodes[i].tt = sd.tt;
		nodes[i].common = common;
		if (*prev_pv != 0) {
			nodes[i].forced_pv = *prev_pv;
			++prev_pv;
		}
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

struct pv_store {
	move pvs[100][MAX_PLY];
	unsigned count;
};

static bool
has_same_pv_in_pv_store(const struct pv_store *pv_store, const move *pv)
{
	for (unsigned pv_i = 0; pv_i < pv_store->count; ++pv_i) {
		const move *p0 = pv_store->pvs[pv_i];
		const move *p1 = pv;

		while (*p0 != 0 && *p0 == *p1) {
			++p0;
			++p1;
		}

		if (*p0 == 0 && *p1 == 0)
			return true;
	}

	return false;
}

static int
find_common_prefix_length(const struct pv_store *pv_store)
{
	int length = 0;

	while (pv_store->pvs[0][length] != 0) {
		for (unsigned i = 1; i < pv_store->count; ++i) {
			if (pv_store->pvs[i][length]
			    != pv_store->pvs[0][length])
				return length;
		}
		++length;
	}

	return length;
}

static void
extract_pv(struct pv_store *pv_store,
		struct search_result *result, struct node *root)
{
	int pv_len = 0;
	int iteration = 0;

#ifndef NDEBUG
	char debug_move_stack[0x1000];
	char *c;
#endif

	int extension_location = -1;
next_iteration:

	pv_len = 0;

#ifndef NDEBUG
	c = debug_move_stack;
#endif

	while (root->pv[pv_len] != 0) {
#ifndef NDEBUG
		*c++ = ' ';
		c = print_coor_move(root->pv[pv_len], c,
		    ((pv_len % 2)
		    ? opponent_of(root->debug_player_to_move)
		    : (int)root->debug_player_to_move));
#endif
		result->pv[pv_len] = root->pv[pv_len];
		root[pv_len].forced_pv = root->pv[pv_len];
		pv_store->pvs[pv_store->count][pv_len] = root->pv[pv_len];
		++pv_len;
	}

	result->pv[pv_len] = 0;

	if (!root->common->sd.settings.use_pv_cleanup)
		return;

	pv_store->pvs[pv_store->count][pv_len] = 0;

	if (++iteration > 98)
		return;

	if (root->depth < 3 * PLY)
		return;

	if (pv_len == 0)
		return;

	if (has_same_pv_in_pv_store(pv_store, root->pv)) {
		if (extension_location == 1)
			return;

		if (extension_location != -1)
			root[extension_location].forced_extension = 0;

		int new_ext = find_common_prefix_length(pv_store) - 1;
		if (new_ext < 1)
			return;

		if (extension_location == -1 || new_ext < extension_location)
			extension_location = new_ext;
		else
			--extension_location;
		root[extension_location].forced_extension = 1;

		pv_store->pvs[0][0] = root->best_move;
		pv_store->pvs[0][1] = 0;
		pv_store->count = 1;
		goto next_iteration;
	}

	pv_store->count++;

	for (int i = pv_len; i < node_array_length - 1; ++i)
		root[i].forced_pv = 0;

	if (root->value == 0)
		return;

	if (root->value <= -mate_value || root->value >= mate_value)
		return;

	if (pv_len >= root->depth / PLY - 1)
		return;

	setup_registers();
	root->alpha = -max_value;
	root->beta = max_value;
	root->expected_type = PV_node;
	negamax(root);

	goto next_iteration;
}

struct search_result
search(const struct position *root_pos,
	enum player debug_player_to_move,
	struct search_description sd,
	volatile atomic_flag *run_flag,
	const move *prev_pv)
{
	struct node *nodes;
	struct nodes_common_data common;
	struct node *root_node;
	struct pv_store *pv_store;

	pv_store = xmalloc(sizeof(*pv_store));
	pv_store->count = 0;

	nodes = xaligned_calloc(pos_alignment,
	    sizeof(nodes[0]), node_array_length);
	memset(&common, 0, sizeof common);
	common.run_flag = run_flag;
	common.sd = sd;
	common.debug_root_player_to_move = debug_player_to_move;
	setup_node_array(node_array_length, nodes, sd, &common, prev_pv);
	root_node = setup_root_node(nodes, root_pos);

	if (setjmp(common.terminate_jmp_buf) == 0) {
		setup_registers();
		negamax(root_node);
		extract_pv(pv_store, &common.result, root_node);
		common.result.value = root_node->value;
		common.result.best_move = root_node->best_move;
		common.result.selective_depth = find_selective_depth(nodes);
		common.result.qdepth = find_qdepth(nodes);
	}
	else {
		common.result.is_terminated = true;
	}

	xaligned_free(nodes);
	free(pv_store);
	return common.result;
}

void
init_search(void)
{
	static const int min_depth = (2 * PLY) - 1;

	for (int d = min_depth + 1; d < (int)ARRAY_LENGTH(LMR); ++d) {
		for (int i = 0; i < (int)ARRAY_LENGTH(LMR[d]); ++i) {
			double x = d * (i + 1);
			int r = (int)(log2((x / 22) + 1) * PLY);

			if (r > d - min_depth)
				r = d - min_depth;
			LMR[d][i] = r;
		}
	}
}
