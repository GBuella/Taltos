
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "chess.h"
#include "game.h"

struct history_item {
    struct position *position;
    move move_to_next;
    struct history_item *prev;
    struct history_item *next;
    unsigned time_spent;
    unsigned full_move;
    unsigned half_move;
    player turn;
};

static void history_destroy(struct history_item *item)
{
    while (item != NULL) {
        struct history_item *next = item->next;
        free(item->position);
        free(item);
        item = next;
    }
}

struct game {
    struct history_item head;
    struct history_item *current;
    struct history_item *tail;
    unsigned current_index;
    unsigned item_count;
};

struct game *game_create(void)
{
    struct game *g = malloc(sizeof *g);

    if (g == NULL) return NULL;
    if ((g->head.position = position_create()) == NULL) {
        free(g);
        return NULL;
    }
#ifndef NDEBUG
    const char *t =
#endif
    position_read_fen_full(g->head.position,
                           start_position_fen,
                           &g->head.full_move,
                           &g->head.half_move,
                           &g->head.turn);
    assert(t != NULL);
    g->head.next = NULL;
    g->head.prev = NULL;
    g->item_count = 1;
    g->current_index = 0;
    g->current = &g->head;
    g->tail = &g->head;
    return g;
}

struct game *game_create_position(const struct position *p)
{
    struct game *g = game_create();

    if (g == NULL) return NULL;
    position_copy(g->head.position, p);
    return g;
}

struct game *game_create_fen(const char *str)
{
    if (str == NULL) return NULL;

    struct game *g = game_create();

    if (g == NULL) return NULL;
    if (NULL == position_read_fen_full(g->head.position,
                                       str,
                                       &g->head.full_move,
                                       &g->head.half_move,
                                       &g->head.turn))
    {
        free(g);
        return NULL;
    }
    return g;
}

void game_destroy(struct game* g)
{
    if (g != NULL) {
        free(g->head.position);
        history_destroy(g->head.next);
        free(g);
    }
}

const struct position *game_current_position(const struct game *g)
{
    assert(g != NULL);

    return g->current->position;
}

enum player game_turn(const struct game *g)
{
    assert(g != NULL);

    return g->current->turn;
}

unsigned game_half_move_count(const struct game *g)
{
    assert(g != NULL);

    return g->tail->half_move;
}

unsigned game_full_move_count(const struct game *g)
{
    assert(g != NULL);

    return g->tail->full_move;
}

int game_history_revert(struct game *g)
{
    assert(g != NULL);

    if (g->current->prev == NULL) return -1;
    g->current = g->current->prev;
    g->current_index--;
    return 0;
}

int game_history_forward(struct game *g)
{
    assert(g != NULL);

    if (g->current->next == NULL) return -1;
    g->current = g->current->next;
    g->current_index++;
    return 0;
}

void game_truncate(struct game *g)
{
    assert(g != NULL);

    if (g->current->next == NULL) return;
    history_destroy(g->current->next);
    g->tail = g->current;   
    g->current->next = NULL;
    g->current->move_to_next = 0;
    g->current->time_spent = 0;
    g->item_count = g->current_index + 1;
}

int game_append(struct game *g, move m)
{
    assert(g != NULL);
    assert(is_move_valid(m));

    struct history_item *next = malloc(sizeof(*next));

    if (next == NULL) return -1;
    if (!is_legal_move(g->current->position, m)) {
        return -1;
    }
    if ((next->position = position_create()) == NULL) {
        free(next);
        return -1;
    }
    game_truncate(g);
    position_copy(next->position, g->current->position);
    make_move(next->position, m);
    if (is_move_irreversible(g->current->position, m)) {
        next->half_move = 0;
    }
    else {
        next->half_move = g->current->half_move+1;
    }
    if (g->current->turn == white) {
        next->turn = black;
        next->full_move = g->current->full_move;
    }
    else {
        next->turn = white;
        next->full_move = g->current->full_move + 1;
    }
    next->next = NULL;
    next->move_to_next = 0;
    next->time_spent = 0;
    g->current->next = next;
    g->current->move_to_next = m;
    next->prev = g->current;
    g->current = g->tail = next;
    g->item_count++;
    g->current_index++;
    return 0;
}

char *game_print_fen(const struct game *g, char str[static FEN_BUFFER_LENGTH])
{
    assert(g != NULL);
    assert(str != NULL);

    (void)position_print_fen_full(g->current->position, str,
                                  g->current->full_move,
                                  g->current->half_move,
                                  g->current->turn);
    return str;
}

static struct history_item *hitem_copy(const struct history_item *src)
{
    struct history_item *dst;

    dst = malloc(sizeof *dst);
    if (dst == NULL) return NULL;
    memcpy(dst, src, sizeof *dst);
    dst->position = position_create();
    if (dst->position == NULL) {
        free(dst);
        return NULL;
    }
    position_copy(dst->position, src->position);
    return dst;
}

struct game *game_copy(const struct game *src)
{
    if (src == NULL) return NULL;

    struct game *dst;
    const struct history_item *item;
    struct history_item *dst_item;

    dst = malloc(sizeof *dst);
    if (dst == NULL) return NULL;
    memcpy(dst, src, sizeof *dst);
    dst->head.position = position_create();
    position_copy(dst->head.position, src->head.position);

    item = &src->head;
    dst_item = &dst->head;
    do {
        if (item == src->current) {
            dst->current = dst_item;
        }
        if (item == src->tail) {
            dst->tail = dst_item;
            break;
        }
        if (item->next == NULL) {
            break;
        }
        dst_item->next = hitem_copy(item->next);
        if (dst_item->next == NULL) {
            game_destroy(dst);
            return NULL;
        }
        dst_item = dst_item->next;
        item = item->next;
    } while (item != NULL);
    return dst;
}

bool game_is_ended(const struct game *g)
{
    assert(g != NULL);

    return !has_any_legal_move(g->current->position);
}

move game_get_single_response(const struct game *g)
{
    move moves[MOVE_ARRAY_LENGTH];

    (void)gen_moves(g->current->position, moves);
    return moves[0];
}

bool game_has_single_response(const struct game *g)
{
    move moves[MOVE_ARRAY_LENGTH];

    (void)gen_moves(g->current->position, moves);
    return moves[0] != 0 && moves[1] == 0;
}

