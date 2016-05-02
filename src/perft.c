
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "perft.h"
#include "chess.h"
#include "chess.h"
#include "position.h"
#include "engine.h"
#include "search.h"
#include "util.h"

struct divide_info {
    struct position pos;
    unsigned depth;
    move moves[MOVE_ARRAY_LENGTH];
    move *m;
    char str[32];
    player turn;
    bool is_ordered;
};

unsigned long perft(const struct position *pos, unsigned depth)
{
    assert(pos != NULL);
    assert(depth <= MAX_PLY);

    unsigned long n = 0;
    move moves[MOVE_ARRAY_LENGTH];
    struct position child[1];

    if (depth == 0) {
        return 1;
    }
    (void)gen_plegal_moves(pos, moves);
    for (move *i = moves; *i != 0; ++i) {
        memcpy(child, pos, sizeof child);
        if (make_plegal_move(child, *i) == 0) {
            n += perft(child, depth - 1);
        }
    }
    return n;
}

static unsigned long do_perft_ordered(struct node *node)
{
    struct move_fsm move_order[1];
    unsigned long n;

    if (node->depth == 0) {
        return 1;
    }
    move_fsm_setup(node, move_order);
    if (move_order->plegal_count == 0) return 0;
    if (rand() & 1) {
        unsigned mi = rand() % move_order->plegal_count;

        node->hte = ht_set_move_index(HT_NULL, mi);

        if (rand() % 12 == 0) {
            node->killer = move_order->moves[mi];
        }
        else {
            if (rand() & 1) {
                mi = rand() % move_order->plegal_count;
                node->killer = move_order->moves[mi];
            }
            else {
                node->killer = 0;
            }
        }

    }
    else {
        node->hte = HT_NULL;
    }
    node[1].depth = node->depth - 1;
    n = 0;
    do {
        select_next_move(node, move_order);
        if (move_order->latest_phase == done) {
            return n;
        }
        n += do_perft_ordered(node + 1);
    } while (true);
}

unsigned long perft_ordered(const struct position *pos, unsigned depth)
{
    struct node nodes[depth + 1];

    memset(nodes, 0, (depth + 1) * sizeof *nodes);
    nodes[0].depth = depth;
    position_copy(nodes[0].pos, pos);
    return do_perft_ordered(nodes);
}

unsigned long perft_distinct(const struct position *pos, unsigned depth)
{
    assert(pos != NULL);
    assert(depth > 0 && depth <= MAX_PLY);

    return perft(pos, depth);
}

struct divide_info *
divide_init(const struct position *pos,
            unsigned depth,
            player turn, bool ordered)
{
    assert(pos != NULL);
    assert(depth > 0 && depth <= MAX_PLY);
    assert(turn == white || turn == black);

    struct divide_info *dinfo;

    dinfo = xmalloc(sizeof *dinfo);
    dinfo->depth = depth;
    dinfo->turn = turn;
    dinfo->is_ordered = ordered;
    memcpy(&dinfo->pos, pos, sizeof dinfo->pos);
    (void)gen_moves(pos, dinfo->moves);
    dinfo->m = dinfo->moves;
    return dinfo;
}

const char *divide(struct divide_info *dinfo, enum move_notation_type mn)
{
    assert(dinfo != NULL);
    if (*dinfo->m == 0) return NULL;
    struct position t;
    unsigned long n;

    char *str;

    str = print_move(&dinfo->pos, *dinfo->m, dinfo->str, mn, dinfo->turn);
    memcpy(&t, &dinfo->pos, sizeof t);
    make_move(&t, *dinfo->m);
    if (dinfo->is_ordered) {
        n = perft_ordered(&t, dinfo->depth-1);
    }
    else {
        n = perft(&t, dinfo->depth-1);
    }
    (void)sprintf(str, " %lu", n);
    ++dinfo->m;
    return dinfo->str; 
}

void divide_destruct(struct divide_info *dinfo)
{
    free(dinfo);
}

