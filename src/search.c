
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

enum { node_array_length = MAX_PLY + MAX_Q_PLY + 32 };
enum { nmr_factor = PLY };
enum { SE_extension = PLY };
enum { futility_margin = (4 * pawn_value) / 3 };

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
	bool search_reached;
	bool any_search_reached;
	struct nodes_common_data *common;
	struct move_fsm move_fsm;
	int static_value;
	bool static_value_computed;
	uint64_t opp_king_sliding_reach;
	uint64_t opp_king_knight_reach;
	uint64_t opp_king_pawn_reach;
	bool king_reach_maps_computed;

	enum player debug_player_to_move;
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

	if (node->best_move == 0)
		entry = ht_set_value(entry, vt_upper_bound, node->alpha);
	else if (node->value >= node->beta)
		entry = ht_set_value(entry, vt_lower_bound, node->beta);
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
get_static_value(struct node *node)
{
	if (!node->static_value_computed) {
		node->static_value = eval(node->pos);
		node->static_value_computed = true;
	}

	return node->static_value;
}

static bool
possibly_gives_check(struct node *node, move m)
{
	// TODO: this misses checks by castling
	// TODO: this misses some revealed checks by en-passant

	if (!node->king_reach_maps_computed) {
		int ki = bsf(node->pos->map[opponent_king]);

		node->opp_king_sliding_reach = rook_pattern_table[ki];
		node->opp_king_sliding_reach |= bishop_pattern_table[ki];
		node->opp_king_knight_reach = knight_pattern(ki);
		node->opp_king_pawn_reach =
		    pawn_attacks_opponent(node->pos->map[opponent_king]);

		node->king_reach_maps_computed = true;
	}

	if (is_nonempty(mfrom64(m) & node->opp_king_sliding_reach))
		return true;

	if (mresultp(m) == king)
		return false;
	else if (mresultp(m) == pawn)
		return is_nonempty(mto64(m) & node->opp_king_pawn_reach);
	else if (mresultp(m) == knight)
		return is_nonempty(mto64(m) & node->opp_king_knight_reach);
	else
		return is_nonempty(mto64(m) & node->opp_king_sliding_reach);
}

static int
get_LMR_factor(const struct node *node)
{
	if (!node->common->sd.settings.use_LMR)
		return 0;

	int reduction;

	if ((node->root_distance == 0)
	    || is_in_check(node[0].pos)
	    || is_in_check(node[1].pos)
	    || (node->depth <= PLY)
	    || !node->move_fsm.is_in_late_move_phase
	    || node->move_fsm.count < 16
	    || (node->move_fsm.index < 2))
		return 0;

	if (node->pos->map[0] ==
	    (node->pos->map[king] | node->pos->map[pawn]))
		return 0;

	if (node->depth > 6 * PLY)
		reduction = node->depth / 4;
	else
		reduction = PLY + PLY / 2;

	if (node->move_fsm.index >= node->move_fsm.very_late_moves_begin)
		reduction += PLY / 2;

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

	debug_trace_tree_init(node);

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
}

enum { prune_successfull = 1 };

static int
try_null_move_prune(struct node *node)
{
	if (!node->common->sd.settings.use_null_moves)
		return 0;

	int advantage = get_static_value(node) - node->beta;

	if ((node->expected_type != cut_node)
	    || (advantage <= rook_value)
	    || (node->depth <= PLY)
	    || is_in_check(node->pos)
	    || (node->move_fsm.count < 18))
		return 0;

	if (node->pos->map[0] ==
	    (node->pos->map[king] | node->pos->map[pawn]))
		return 0;

	if (is_nonempty(node->pos->rpin_map)
	    || is_nonempty(node->pos->bpin_map))
		return 0;

	unsigned non_pawn_moves = 0;
	unsigned non_pawn_moving = 0;

	bool moving_piece[PIECE_ARRAY_SIZE] = {0};

	for (unsigned i = 0; i < node->move_fsm.count; ++i) {
		int from = mfrom(node->move_fsm.moves[i]);
		int p = pos_piece_at(node->pos, from);

		if (p != pawn) {
			++non_pawn_moves;
			if (!moving_piece[p]) {
				++non_pawn_moving;
				moving_piece[p] = true;
			}
		}
	}

	if (non_pawn_moves < 18 || non_pawn_moving < 2)
		return 0;

	struct node *child = node + 1;

	position_flip(child->pos, node->pos);

	if (is_nonempty(child->pos->rpin_map)
	    || is_nonempty(child->pos->bpin_map)) {
		return 0;
	}

	child->expected_type = all_node;
	child->alpha = -node->beta - pawn_value - 1;
	child->beta = -node->beta - pawn_value;

	child->depth = node->depth - PLY - nmr_factor;
	child->depth -= ((advantage - rook_value) / pawn_value) * (PLY / 2);

	debug_trace_tree_push_move(node, 0);
	negamax(child);
	debug_trace_tree_pop_move(node);

	int value = -child->value;

	if (value > node->beta + pawn_value && value <= mate_value) {
		node->value = -child->value;
		return prune_successfull;
	}

	return 0;
}

static int
futility_prune(struct node *node, move m)
{
	if (!node->common->sd.settings.use_FP)
		return 0;

	if (node->root_distance < 3)
		return 0;

	if (is_in_check(node->pos))
		return 0;

	if (node->depth > PLY)
		return 0;

	if (node->value <= -mate_value
	    || node->alpha >= mate_value
	    || node->move_fsm.count < 3
	    || is_promotion(m)
	    || possibly_gives_check(node, m))
		return 0;

	int value_guess = get_static_value(node)
	    + ((3 * piece_value[mcapturedp(m)]) / 2);

	int margin = futility_margin;

	if (node->expected_type == PV_node)
		margin *= 2;

	if (mresultp(m) == queen || mcapturedp(m) == queen)
		margin *= 2;
	else if (mresultp(m) == pawn && mcapturedp(m) == 0)
		margin /= 2;

	if (value_guess >= node->alpha - margin)
		return 0;

	return prune_successfull;
}

enum { hash_cutoff = 1 };

static int
check_hash_value(struct node *node, ht_entry entry)
{
	if (node->depth > ht_depth(entry)) {
		if (ht_value_type(entry) == vt_lower_bound
		    || ht_value_type(entry) == vt_exact) {
			if (ht_value(entry) >= mate_value) {
				node->value = ht_value(entry);
				node->best_move = ht_move(entry);
				return hash_cutoff;
			}
		}
		if (ht_value_type(entry) == vt_upper_bound
		    || ht_value_type(entry) == vt_exact) {
			if (ht_value(entry) <= -mate_value) {
				node->value = ht_value(entry);
				if (node->best_move == 0)
					node->best_move = ht_move(entry);
				return hash_cutoff;
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
				return hash_cutoff;
			}
		}
		break;
	case vt_upper_bound:
		if (node->upper_bound > ht_value(entry)) {
			node->upper_bound = ht_value(entry);
			if (node->upper_bound <= node->alpha) {
				node->value = node->upper_bound;
				return hash_cutoff;
			}
		}
		break;
	case vt_exact:
		node->value = ht_value(entry);
		node->best_move = ht_move(entry);
		return hash_cutoff;
	default:
		break;
	}
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
	node->deep_entry = 0;
	node->fresh_entry = 0;

	ht_entry entry = ht_lookup_deep(node->tt, node->pos,
	    node->depth, node->beta);
	if (ht_is_set(entry)
	    && (move_fsm_add_hash_move(&node->move_fsm, ht_move(entry)) == 0)) {
		node->deep_entry = entry;
		if (check_hash_value(node, entry) == hash_cutoff)
			return hash_cutoff;
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

enum { is_trivial_draw = 1 };

static int
check_trivial_draw(struct node *node)
{
	if (has_insufficient_material(node->pos) || is_repetition(node)) {
		node->value = 0;
		return is_trivial_draw;
	}

	return 0;
}

static bool
should_do_singular_extension(struct node *node, move best_move)
{
	if (!node->common->sd.settings.use_SE || node->root_distance == 0)
		return false;

	// Need to have entries in TT already
	if (node->depth < 7 * PLY)
		return false;

	// Only consider the first move, and only if it is a move from TT
	if (node->move_fsm.index != 1)
		return false;

	// Don't lose a deeper entry
	if (ht_depth(node->fresh_entry) >= node->depth)
		return false;

	// Too few moves, it might be too easy to find a singular move
	if (node->move_fsm.count < 6)
		return false;

	if (!node->move_fsm.has_hashmove)
		return false;

	// Suppose a capture can too easily be considered singular
	if (mcapturedp(best_move) == queen
	    || mcapturedp(best_move) == rook
	    || mcapturedp(best_move) == knight
	    || mcapturedp(best_move) == bishop
	    || is_promotion(best_move))
		return false;

	int value;

	int min_depth = node->depth / 2;

	if (ht_is_set(node->deep_entry)
	    && (ht_value_type(node->deep_entry) & vt_lower_bound) != 0
	    && ht_has_move(node->deep_entry)
	    && ht_move(node->deep_entry) == best_move
	    && ht_depth(node->deep_entry) >= min_depth) {
		value = ht_value(node->deep_entry);
	}
	else if (ht_is_set(node->fresh_entry)
	    && (ht_value_type(node->fresh_entry) & vt_lower_bound) != 0
	    && ht_has_move(node->fresh_entry)
	    && ht_move(node->fresh_entry) == best_move
	    && ht_depth(node->fresh_entry) >= min_depth) {
		value = ht_value(node->fresh_entry);
	}
	else {
		return false;
	}

	// TODO: probably there is no need to check this
	if (value <= -mate_value || value >= mate_value)
		return false;

	value -= pawn_value / 2;

	if (mcapturedp(best_move) == pawn)
		value -= pawn_value / 2;

	value -= ((node->depth * (pawn_value / 10)) / PLY);

	struct node *child = node + 1;
	int child_value;

	/*
	 * The first move is already fetch from the move_fsm.
	 * Loop over all other moves, and if they all are that much worse
	 * than the best move, return true.
	 */
	do {
		move m = select_next_move(node->pos, &node->move_fsm);

		make_move(child->pos, node->pos, m);
		debug_trace_tree_push_move(node, m);

		child->expected_type = all_node;
		child->depth = node->depth / 2 - PLY;
		child->alpha = -value;
		child->beta = -value + 1;

		negamax(child);
		child_value = -child->value;

		debug_trace_tree_pop_move(node);

	} while (child_value < value && !move_fsm_done(&node->move_fsm));

	move_fsm_reset(node->pos, &node->move_fsm, 1);

	return child_value < value;
}

static void
setup_child_node(struct node *node, move m, int *LMR_factor)
{
	struct node *child = node + 1;

#ifdef SEARCH_CHECK_REPETITIONS
	child->is_GHI_barrier = is_move_irreversible(node->pos, m);
#endif

	bool use_SE = should_do_singular_extension(node, m);

	make_move(child->pos, node->pos, m);
	debug_trace_tree_push_move(node, m);
	handle_node_types(node);

	child->depth = node->depth - PLY;
	child->beta = -node->alpha;
	child->alpha = -node->beta;

	if (use_SE) {
		child->depth += SE_extension;
		*LMR_factor = 0;
	}
	else if (is_in_check(child->pos)) {
		if (node->depth > PLY)
			child->depth += PLY / 2;
		*LMR_factor = 0;
	}
	else {
		*LMR_factor = get_LMR_factor(node);

		if (*LMR_factor != 0) {
			child->depth -= *LMR_factor;
			child->alpha = -node->alpha - 1;
		}
	}
}

static void
reset_child_after_lmr(struct node *node)
{
	node[1].depth = node->depth - PLY;
	node[1].alpha = -node->beta;
	node[1].beta = -node->alpha;
}

static int
negamax_child(struct node *node)
{
	negamax(node + 1);

	int value = -node[1].value;

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

	if (try_null_move_prune(node) == prune_successfull)
		return;

	if (fetch_hash_value(node) == hash_cutoff)
		return;

	if (recheck_bounds(node) == alpha_not_less_than_beta)
		return;

	do {
		move m = select_next_move(node->pos, &node->move_fsm);

		if (futility_prune(node, m) == prune_successfull)
			continue;

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

		if (value > node->value) {
			node->value = value;
			if (value > node->alpha) {
				node->alpha = value;
				node->best_move = m;
				if (node->alpha >= node->beta) {
					fail_high(node);
					debug_trace_tree_pop_move(node);
					break;
				}
				node->expected_type = PV_node;
			}
		}
		debug_trace_tree_pop_move(node);
	} while (search_more_moves(node));

	ht_entry entry = hash_current_node_value(node);
	setup_best_move(node);
	if (node->depth > 0) {
		if (node->best_move != 0)
			entry = ht_set_move(entry, node->best_move);
		ht_pos_insert(node->tt, node->pos, entry);
	}
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
	enum player debug_player_to_move,
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
	common.debug_root_player_to_move = debug_player_to_move;
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
