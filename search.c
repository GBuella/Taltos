
#include <assert.h>
#include <string.h>

#include "macros.h"
#include "search.h"
#include "position.h"
#include "hash.h"
#include "taltos_threads.h"
#include "trace.h"

uintmax_t node_count;
uintmax_t first_move_cutoff_count;
uintmax_t cutoff_count;

#define NON_VALUE INT_MIN
#define HISTORY_MAX 64
#define CHILD_ANALYZE_DEPTH_MIN (10*PLY)

enum return_values { ht_hit = 1, ht_no_hit, cutoff, leaf_reached };

typedef enum return_values return_values;

void reset_node_counts(void)
{
    node_count = 0;
    first_move_cutoff_count = 0;
    cutoff_count = 0;
}

uintmax_t get_node_count(void)
{
    return node_count;
}

int get_fmc_percent(void)
{
    if (cutoff_count != 0) {
        return (int)((first_move_cutoff_count * 1000) / cutoff_count);
    }
    else {
        return -1;
    }
}

static int negamax(struct node *);

static int max(int a, int b)
{
    return (a > b) ? a : b;
}

static int min(int a, int b)
{
    return (a < b) ? a : b;
}

static bool hash_depth_ok(int entry_depth, int node_depth)
{
    if (entry_depth >= node_depth) return true;
    if ((entry_depth <= PLY && entry_depth > 0 && node_depth <= PLY)) {
        /* Accept fractional ply below one PLY at one PLY,
           since there are no depth reductions at horizon nodes,
           so the same nodes would be searched at e.g. 1 PLY and at 1/2 PLY */

        return true;
    }
    return false;
}

static return_values check_hte(struct node *node, ht_entry entry)
{
        /* No fail high or fail low cutoffs at root node, 
           to make sure a best move is returned */
    if (  ((ht_value_type(entry) == vt_exact && ht_has_move(entry))
            || !node->is_search_root)

         /* When being strict about repetitions, hash values are 
            only acceptible following irreversible moves */
            && (node->is_GHI_barrier || !node->sd.strict_repetitions) 


            && hash_depth_ok(ht_depth(entry), node->depth))
    {
        switch (ht_value_type(entry)) {
            case vt_upper_bound:
                if (node->alpha >= ht_value(entry)) {
                    node->value = ht_value(entry);
                    return cutoff;
                }
                if (ht_value(entry) < node->beta) {
                    node->beta = ht_value(entry);
                }
                break;
            case vt_lower_bound:
                if (node->beta <= ht_value(entry)) {
                    node->value = ht_value(entry);
                    return cutoff;
                }
                if (ht_value(entry) > node->alpha) {
                    node->alpha = ht_value(entry);
                }
                break;
            case vt_exact:
                node->value = ht_value(entry);
                return cutoff;
            default: break;
        }
    }
    return 0;
}

static return_values check_ht(struct node *node, struct move_fsm *ml)
{
    ht_entry entry;
        
    bool has = false;
    node->hte = ht_pos_lookup(node->sd.ht_main, node->pos, ml->plegal_count);
    entry = node->hte;
    if (ht_is_set(entry)) { has = true;
        if (ht_has_move(entry)) {
            node->hash_move_i = ht_move_index(entry);
        }
        if (check_hte(node, entry) == cutoff) {
            return cutoff;
        }
    }
    entry = ht_pos_lookup(node->sd.ht_aux, node->pos, ml->plegal_count);
    if (ht_is_set(entry)) { has = true;
        if (node->hash_move_i == -1 && ht_has_move(entry)) {
            node->hash_move_i = ht_move_index(entry);
        }
        if (check_hte(node, entry) == cutoff) {
            return cutoff;
        }
    }
    if (node->depth > 16 && !has) {
        trace("Node Has no entry found at d:%d", node->depth);
        trace_node(node, ml);
    }
    return 0;
}

static bool is_repetition(const struct node *node)
{
    zobrist_hash hash = node->pos->hash[1];

    while (!node->is_GHI_barrier) {
        node -= 2;
        if (node->pos->hash[1] == hash) return true;
    }
    return false;
}

static int node_init(struct node *node, struct move_fsm *ml)
{
    node->hash_move_i = -1;
    node->best_move_index = -1;
    if (node->sd.strict_repetitions) {
        if (is_repetition(node)) {
            node->value = 0;
            return cutoff;
        }
    }
    if (node->root_distance >= MAX_PLY + MAX_Q_PLY - 1) {
        node->value = eval(node);
        return leaf_reached;
    }
    if (in_check(node->pos)) {
        node->depth = max(node->depth + 1, PLY);
    }
    move_fsm_setup(node, ml);
    if (ml->plegal_count == 0) {
        if (is_qsearch(node)) {
            node->value = eval(node);
        }
        else {
            node->value = in_check(node->pos) ? -MATE_VALUE-MAX_PLY : 0;
        }
        return leaf_reached;
    }
    if (!in_check(node->pos) && ml->plegal_count < 10 && !is_qsearch(node)) {
        if (node->depth < 50 && node->depth % PLY != 0) {
            node->depth = ((node->depth / PLY) + 1) * PLY;
        }
    }
    if (check_ht(node, ml) == cutoff) {
        if (node->hash_move_i != -1) {
            node->best_move_index = node->hash_move_i;
            node->best_move = ml->moves[node->hash_move_i];
        }
        return cutoff;
    }
    if (is_qsearch(node)) {
        int value = eval(node);
        if (value > node->alpha) {
            node->value = value;
            if (value >= node->beta) {
                return cutoff;
            }
            node->alpha = value;
        }
        else if (value < node->alpha - QUEEN_VALUE) {
            node->value = node->alpha;
            return cutoff;
        }
    }
    node->value = NON_VALUE;
    return 0;
}

static bool has_best_move(const struct node *node)
{
    return node->best_move_index >= 0;
}

static ht_entry set_hash_node_value(const struct node *node, ht_entry e)
{
    if (node->value != NON_VALUE) {
        if (has_best_move(node)) {
            if (node->value >= node->beta) {
                return ht_set_value(e, vt_lower_bound, node->value);
            }
            else {
                return ht_set_value(e, vt_exact, node->value);
            }
        }
        else {
            return ht_set_value(e, vt_upper_bound, node->value);
        }
    }
    else {
        return ht_set_value(e, vt_upper_bound, node->alpha);
    }
}

static ht_entry hash_current_node(const struct node *node)
{
    ht_entry entry = HT_NULL;

    entry = ht_set_depth(HT_NULL, node->depth);
    entry = set_hash_node_value(node, entry);
    if (has_best_move(node)) {
        entry = ht_set_move_index(entry, node->best_move_index);
    }
    else {
        entry = ht_set_no_move(entry);
    }
    return entry;
}

static bool hash_has_stricter_value(ht_entry a, ht_entry b)
{
    switch (ht_value_type(b)) {
        case vt_exact: return true;
        case vt_none: return false;
        case vt_upper_bound:
            if (ht_value_type(a) == vt_upper_bound) {
                return ht_value(b) < ht_value(a);
            }
            return false;
        case vt_lower_bound:
            if (ht_value_type(a) == vt_lower_bound) {
                return ht_value(b) > ht_value(a);
            }
            return false;
    }
}

static void save_node_hash_regular(const struct node *node)
{
    ht_entry entry = hash_current_node(node);
    ht_entry old = node->hte;

    if (is_qsearch(node)) {
        ht_pos_insert(node->sd.ht_aux, node->pos, entry);
    }
    else if (ht_is_set(old)) {
        if (ht_depth(old) > node->depth) {
            ht_pos_insert(node->sd.ht_aux, node->pos, entry);
        }
        else if (ht_depth(old) == node->depth) {
            if (hash_has_stricter_value(old, entry)) {
                if (ht_has_move(old) && !ht_has_move(entry)) {
                    entry = ht_reset_move(entry, old);
                }
                ht_pos_insert(node->sd.ht_main, node->pos, entry);
            }
            else {
                ht_pos_insert(node->sd.ht_aux, node->pos, entry);
            }
        }
        else {
            if (ht_has_move(old) && !ht_has_move(entry)) {
                entry = ht_reset_move(entry, old);
            }
            ht_pos_insert(node->sd.ht_main, node->pos, entry);
            if (hash_has_stricter_value(entry, old)) {
                ht_pos_insert(node->sd.ht_aux, node->pos, old);
            }
        }
    }
    else {
        ht_pos_insert(node->sd.ht_main, node->pos, entry);
    }
}

static void save_node_hash_move_only(const struct node *node)
{
    ht_entry entry;

    if (!has_best_move(node)) return;
    entry = ht_set_move_index(HT_NULL, node->best_move_index);
    if (ht_is_set(node->hte) && ht_has_move(node->hte)) {
        if (ht_depth(node->hte) > node->depth) return;
        if (node->best_move_index != (int)ht_move_index(node->hte)) {
            ht_pos_insert(node->sd.ht_main, node->pos, entry);
        }
    }
    else {
        ht_pos_insert(node->sd.ht_main, node->pos, entry);
    }
}

static void save_node_hash(const struct node *node)
{
    if (node->is_GHI_barrier || !node->sd.strict_repetitions) {
        save_node_hash_regular(node);
    }
	else {
        save_node_hash_move_only(node);
    }
}

static bool is_first_child(const struct move_fsm *ml)
{
    return ml->legal_counter == 1;
}

static void handle_node_types(struct node *node, const struct move_fsm *ml)
{
    struct node *child = node + 1;

    switch (node->expected_type) {
        case PV_node:
            child->expected_type = is_first_child(ml) ? PV_node : cut_node;
            break;
        case all_node:
            child->expected_type = cut_node;
            break;
        case cut_node:
            if (!is_first_child(ml) && ml->latest_phase > killer) {
                node->expected_type = all_node;
                child->expected_type = cut_node;
            }
            else {
                child->expected_type = all_node;
            }
            break;
        default:
            unreachable();
    }
}

static bool need_scout_search(const struct node *node)
{
    return node->beta > node->alpha + 1 && node[1].expected_type != PV_node;
}

static bool need_lmr(const struct node *node, const struct move_fsm *ml)
{
    return (node->sd.lmr_factor > 0
            && !is_qsearch(node)
            && node->depth > node->sd.lmr_factor
            && node->expected_type == all_node
            && ml->plegal_count > 20
            && !in_check(node->pos)
            && ml->legal_counter > 2
            && ml->latest_phase > killer);
}

static void fail_high(struct node *node, const struct move_fsm *ml)
{
    if (node->depth > 0 && node->hash_move_i == -1) {
        ++cutoff_count;
        if (is_first_child(ml)) {
            ++first_move_cutoff_count;
        }
        else if (false) {
            trace("Late move fail high\n");
            trace_node(node, ml);
        }
    }
    if (ml->latest_phase == general) {
        node->killer = node->best_move;
    }
}

static void search_child(struct node *node, const struct move_fsm *ml)
{
    int value;
    struct node *child = node + 1;
    bool scout = need_scout_search(node);
    bool lmr = need_lmr(node, ml);

    child->alpha = scout ? -(node->alpha + 1) : -node->beta;
    child->beta = -node->alpha;
    child->depth = node->depth - PLY;
    if (lmr) {
        child->depth -= node->sd.lmr_factor;
        if (ml->legal_counter > 13) {
            child->depth -= node->sd.lmr_factor;
            if (ml->legal_counter > 20) {
                child->depth -= node->sd.lmr_factor;
            }
        }
    }
    value = -negamax(child);
    if (value > node->alpha) {
        if (lmr) {
            child->alpha = scout ? -(node->alpha + 1) : -node->beta;
            child->beta = -node->alpha;
            child->depth = node->depth - PLY;
            value = -negamax(child);
            if (value > node->alpha && value < node->beta && scout) {
                child->alpha = -node->beta;
                child->beta = -node->alpha;
                child->depth = node->depth - PLY;
                child->expected_type = PV_node;
                value = -negamax(child);
            }
        }
        else if (scout && value < node->beta ) {
            child->alpha = -node->beta;
            child->beta = -node->alpha;
            child->depth = node->depth - PLY;
            child->expected_type = PV_node;
            value = -negamax(child);
        }
    }
    if (value > node->alpha) {
        if (value > MATE_VALUE) --value;
        node->value = value;
        node->alpha = value;
        node->best_move_index = ml->index;
        node->best_move = node->current_move;
        
        if (value >= node->beta) {
            fail_high(node, ml);
        }
        else {
            node->expected_type = PV_node;
        }
    }
    else if (node->value == NON_VALUE) {
        node->value = value;
    }
    else if (value > node->value) {
        node->value = value;
    }
}

static bool
tempo_waster_prune(const struct node *node, const struct move_fsm *ml)
{
    if ((!node->sd.twp)
            || is_qsearch(node)
            || node->root_distance < 4
            || in_check(node->pos)
            || ml->plegal_count < 24
            || node->expected_type != all_node
            || is_first_child(ml)
            || ml->latest_phase < general
            || node[1].is_GHI_barrier)
    {
        return false;
    }

    const struct node *n = node - 2;
    if (n[1].is_GHI_barrier) return false;

    move r = move_revert(node->current_move);
    int i = 0;

    do {
        if (!in_check(n->pos) && move_match(n->current_move, r)) {
            return true;
        }
        ++i;
        n -= 2;
    } while (i < 5 && !n[1].is_GHI_barrier);
    return false;
}

static bool
can_attempt_null_move(const struct node *node, const struct move_fsm *ml)
{
    if (node->sd.nmr_factor > 0
            && node->expected_type == cut_node
            && node->depth > PLY
            && !in_check(node->pos)
            && ml->plegal_count > 20)
    {
        if (!ht_is_set(node->hte)) return true;
        if (ht_depth(node->hte) < node->depth - node->sd.nmr_factor) {
            return true;
        }
        if (ht_value_type(node->hte) == vt_exact
                || ht_value_type(node->hte) == vt_upper_bound)
        {
            return ht_value(node->hte) >= node->beta;
        }
        else {
            return true;
        }
    }
    else {
        return false;
    }
}

static int null_move_prune(struct node *node)
{
    int material_value = eval_material(node->pos->bb);

    if (material_value < node->beta + PAWN_VALUE) return -1;
    position_flip(node[1].pos, node->pos);
    node->current_move = 0;
    node[1].expected_type = all_node;
    node[1].depth = node->depth - node->sd.nmr_factor;
    node[1].depth -= min((material_value - node->beta) / 2*PAWN_VALUE, 2*PLY);
    node[1].alpha = -node->beta - 1;
    node[1].beta = -node->beta;
    if (-negamax(node+1) >= node->beta) {
        return 0;
    }
    else {
        return -1;
    }
}

static int negamax(struct node *node)
{
    assert(node->alpha < node->beta);

    struct move_fsm ml[1];

    thread_cancel_point();
    ++node_count;
    node->any_search_reached = true;
    if (node->depth > 0 && (node-1)->depth > 0) {
        node->search_reached = true;
    }
    if (node_init(node, ml) != 0) {
        return node->value;
    }
    if (can_attempt_null_move(node, ml)) {
        if (null_move_prune(node) == 0) {
            return node->beta;
        }
    }
    do {
        select_next_move(node, ml);
        if (ml->latest_phase == done) break;
        handle_node_types(node, ml);
        if (tempo_waster_prune(node, ml)) {
            continue;
        }
        trace_node_count_before(node);
        search_child(node, ml);
        trace_node_count_after(node);
    } while (node->alpha < node->beta);
    if (is_qsearch(node)) {
        if (node->alpha < node->beta) {
            int value = eval(node);
            if (ml->legal_counter == 0) {
                return value;
            }
            else if (value > node->alpha) {
                node->value = value;
            }
        }
    }
    else if (ml->legal_counter == 0) {
        return in_check(node->pos) ? -MATE_VALUE - MAX_PLY : 0;
    }
    save_node_hash(node);
    return (node->value == NON_VALUE) ? node->alpha : node->value;
}

int search(const struct position *pos,
           move *best_move,
           struct search_description sd,
           int *selective_depth,
           int *qdepth)
{
    assert(pos != NULL);
    assert(best_move != NULL);

    int value;
    struct node nodes[MAX_PLY + MAX_Q_PLY + 32];

    memset(nodes, 0, sizeof nodes);
    for (unsigned i = 1; i < ARRAY_LENGTH(nodes); ++i) {
        nodes[i].root_distance = i - 1;
        nodes[i].sd = sd;
    }
    nodes[0].search_reached = true;
    nodes[1].search_reached = true;
    nodes[0].any_search_reached = true;
    nodes[1].any_search_reached = true;
    nodes[0].is_GHI_barrier = true;
    nodes[0].is_search_root = true;
    nodes[1].is_GHI_barrier = true;
    nodes[1].alpha = -MAX_VALUE;
    nodes[1].beta = MAX_VALUE;
    nodes[1].depth = sd.depth;
    nodes[1].is_search_root = true;
    nodes[1].expected_type = PV_node;
    position_copy(nodes[1].pos, pos);
    value = negamax(nodes + 1);
    *best_move = nodes[1].best_move;
    int d;
    for (d = 1; nodes[d].search_reached; ++d);
    d -= 2;
    d *= PLY;
    *selective_depth = (d > sd.depth) ? d : sd.depth;
    for (d = 1; nodes[d].any_search_reached; ++d);
    d -= 2;
    *qdepth = d;
    trace("Hash usage at end of search: %u%%\n", ht_usage(sd.ht_main));
    return value;
}

