
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "chess.h"
#include "game.h"
#include "util.h"

struct history_item {
	struct position *pos;
	move move_to_next;
	struct history_item *prev;
	struct history_item *next;
	unsigned time_spent;
	unsigned full_move;
	unsigned half_move;
	int ep_target_index;
	enum player turn;
};

static void
history_destroy(struct history_item *item)
{
	while (item != NULL) {
		struct history_item *next = item->next;
		position_destroy(item->pos);
		free(item);
		item = next;
	}
}

struct game {
	struct history_item head;
	struct history_item *current;
	struct history_item *tail;
	size_t current_index;
	size_t item_count;
};

struct game*
game_create(void)
{
	const char *c;
	struct game *g = xmalloc(sizeof *g);
	g->head.pos = position_allocate();

	c = position_read_fen_full(g->head.pos,
	    start_position_fen,
	    &g->head.ep_target_index,
	    &g->head.full_move,
	    &g->head.half_move,
	    &g->head.turn);
	assert(c != NULL);
	(void) c; // the starting position FEN is assumed to be valid...
	g->head.next = NULL;
	g->head.prev = NULL;
	g->item_count = 1;
	g->current_index = 0;
	g->current = &g->head;
	g->tail = &g->head;
	return g;
}

struct game*
game_create_position(const struct position *p)
{
	struct game *g = game_create();

	g->head.pos = position_dup(p);
	return g;
}

struct game*
game_create_fen(const char *str)
{
	struct game *g = game_create();

	if (NULL == position_read_fen_full(g->head.pos,
	    str,
	    &g->head.ep_target_index,
	    &g->head.full_move,
	    &g->head.half_move,
	    &g->head.turn)) {
		game_destroy(g);
		return NULL;
	}
	return g;
}

void
game_destroy(struct game *g)
{
	if (g != NULL) {
		history_destroy(g->head.next);
		position_destroy(g->head.pos);
		free(g);
	}
}

const struct position*
game_current_position(const struct game *g)
{
	return g->current->pos;
}

enum player
game_turn(const struct game *g)
{
	return g->current->turn;
}

unsigned
game_half_move_count(const struct game *g)
{
	return g->tail->half_move;
}

unsigned
game_full_move_count(const struct game *g)
{
	return g->tail->full_move;
}

const struct position*
game_history_position(const struct game *g, int delta)
{
	const struct history_item *item = g->current;
	while (delta > 0) {
		if (item->prev == NULL)
			return NULL;
		item = item->prev;
		--delta;
	}
	while (delta < 0) {
		if (item->next == NULL)
			return NULL;
		item = item->next;
		++delta;
	}
	return item->pos;
}

int
game_history_revert(struct game *g)
{
	if (g->current->prev == NULL)
		return -1;
	g->current = g->current->prev;
	g->current_index--;
	return 0;
}

int
game_history_forward(struct game *g)
{
	if (g->current->next == NULL)
		return -1;
	g->current = g->current->next;
	g->current_index++;
	return 0;
}

void
game_truncate(struct game *g)
{
	if (g->current->next == NULL)
		return;
	history_destroy(g->current->next);
	g->tail = g->current;
	g->current->next = NULL;
	g->current->move_to_next = 0;
	g->current->time_spent = 0;
	g->item_count = g->current_index + 1;
}

int
game_append(struct game *g, move m)
{
	if (!is_legal_move(g->current->pos, m))
		return -1;

	struct history_item *next = xmalloc(sizeof *next);
	next->pos = position_allocate();
	game_truncate(g);
	position_make_move(next->pos, g->current->pos, m);
	next->ep_target_index = 0;
	if (is_move_irreversible(g->current->pos, m)) {
		if (mtype(m) == mt_pawn_double_push) {
			if (g->current->turn == white)
				next->ep_target_index = flip_i(mto(m));
			else
				next->ep_target_index = mto(m);
		}
		next->half_move = 0;
	}
	else {
		next->half_move = g->current->half_move + 1;
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

char*
game_print_fen(const struct game *g, char str[static FEN_BUFFER_LENGTH])
{
	(void) position_print_fen_full(g->current->pos, str,
	    g->current->ep_target_index,
	    g->current->full_move,
	    g->current->half_move,
	    g->current->turn);
	return str;
}

static struct history_item*
hitem_copy(const struct history_item *src)
{
	struct history_item *dst;

	dst = xmalloc(sizeof *dst);
	memcpy(dst, src, sizeof *dst);
	dst->pos = position_dup(src->pos);
	return dst;
}

struct game*
game_copy(const struct game *src)
{
	assert(src != NULL);

	struct game *dst;
	const struct history_item *item;
	struct history_item *dst_item;

	dst = xmalloc(sizeof *dst);
	memcpy(dst, src, sizeof *dst);
	dst->head.pos = position_dup(src->head.pos);

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
		dst_item = dst_item->next;
		item = item->next;
	} while (item != NULL);
	return dst;
}

bool
game_is_ended(const struct game *g)
{
	return !has_any_legal_move(g->current->pos);
}

move
game_get_single_response(const struct game *g)
{
	move moves[MOVE_ARRAY_LENGTH];

	(void) gen_moves(g->current->pos, moves);
	return moves[0];
}

bool
game_has_single_response(const struct game *g)
{
	move moves[MOVE_ARRAY_LENGTH];

	(void) gen_moves(g->current->pos, moves);
	return moves[0] != 0 && moves[1] == 0;
}
