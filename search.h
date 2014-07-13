
#ifndef SEARCH_H
#define SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#include "chess.h"
#include "engine.h"
#include "position.h"
#include "hash.h"
#include "time.h"

#define MAX_VALUE 0x7ff
#define MATE_VALUE (MAX_VALUE - 100)
#define PAWN_VALUE 0x10
#define KNIGHT_VALUE 0x30
#define BISHOP_VALUE 0x31
#define ROOK_VALUE 0x50
#define QUEEN_VALUE 0x90
#define XQUEEN_VALUE (QUEEN_VALUE - BISHOP_VALUE - ROOK_VALUE)

extern const int piece_value[8];

enum move_order_phase {
    initial,
    hash_move,
    tactical_moves,
    killer,
    general,
    losing_moves,
    done
};

struct move_fsm {
    move moves[MOVE_ARRAY_LENGTH];
    int value[MOVE_ARRAY_LENGTH];
    unsigned plegal_count;
    unsigned plegal_remaining;
    unsigned legal_counter;
    enum move_order_phase latest_phase;
    int killer_i;
    unsigned index;
    uint64_t king_b_reach;
    uint64_t king_kn_reach;
};

enum node_type {
    unknown = 0,
    PV_node,
    all_node,
    cut_node
};

struct search_description {
    struct hash_table *ht_main;
    struct hash_table *ht_aux;
    struct hash_table *ht_captures;
    bool strict_repetitions;
    int depth;
    bool uses_timer;
    int depth_limit;
    clock_t thinking_started;
    int lmr_factor; /* Late move reduction */
    bool twp; /* Tempo waster prune */
    int nmr_factor; /* null-move heuristic reduction */
    bool threat_extension;
};

struct node {
    struct position pos[1];
    unsigned root_distance;
    enum node_type expected_type;
    int alpha;
    int beta;
    int depth;
    bool is_search_root;
    ht_entry hte;
    int best_move_index;
    int hash_move_i;
    move best_move;
    move killer;
    bool is_GHI_barrier;
    struct search_description sd;
    move current_move;
    unsigned char history[8][64];
    bool need_to_reset_history;
    int value;
    bool search_reached;
    bool any_search_reached;
#   ifdef SEARCH_TRACE_PATH
    uintmax_t node_count_pivot;
#   endif
};

int search(const struct position *,
           move *best_move,
           struct search_description sd,
           int *selective_depth,
           int *qdepth);
void move_fsm_setup(const struct node *, struct move_fsm *);
void select_next_move(struct node *, struct move_fsm *);
int eval(const struct node *node);
int eval_material(const uint64_t bb[static 5]);

static inline bool is_qsearch(const struct node *node)
{
    return node->depth <= 0;
}

#endif
