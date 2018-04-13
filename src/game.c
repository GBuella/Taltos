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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "chess.h"
#include "game.h"
#include "util.h"

struct history_item {
	struct position *pos;
	struct move move_to_next;
	struct history_item *prev;
	struct history_item *next;
	unsigned time_spent;
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
	struct game *g = xmalloc(sizeof *g);
	g->head.pos = NULL;
	g->head.next = NULL;
	g->current = &g->head;
	int r;

	r = game_reset_fen(g, start_position_fen);
	(void) r;
	assert(r == 0);

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

	if (game_reset_fen(g, str) != 0) {
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
	return position_turn(g->current->pos);
}

unsigned
game_half_move_count(const struct game *g)
{
	return position_half_move_count(g->tail->pos);
}

unsigned
game_full_move_count(const struct game *g)
{
	return position_full_move_count(g->tail->pos);
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

struct move
game_move_to_next(const struct game *g)
{
	return g->current->move_to_next;
}

void
game_truncate(struct game *g)
{
	if (g->current->next == NULL)
		return;
	history_destroy(g->current->next);
	g->tail = g->current;
	g->current->next = NULL;
	g->current->move_to_next = null_move();
	g->current->time_spent = 0;
	g->item_count = g->current_index + 1;
}

int
game_append(struct game *g, struct move m)
{
	if (!is_legal_move(g->current->pos, m))
		return -1;

	struct history_item *next = xmalloc(sizeof *next);
	next->pos = position_allocate();
	game_truncate(g);
	make_move(next->pos, g->current->pos, m);
	next->next = NULL;
	next->move_to_next = null_move();
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
	return position_print_fen(str, g->current->pos);
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
	return game_is_draw_by_repetition(g)
	    || game_is_draw_by_insufficient_material(g)
	    || game_is_draw_by_50_move_rule(g)
	    || !has_any_legal_move(g->current->pos);
}

bool
game_is_checkmate(const struct game *g)
{
	return pos_is_check(g->current->pos)
	    && !has_any_legal_move(g->current->pos);
}

bool
game_is_stalemate(const struct game *g)
{
	return !pos_is_check(g->current->pos)
	    && !has_any_legal_move(g->current->pos);
}

bool
game_is_draw_by_insufficient_material(const struct game *g)
{
	return pos_has_insufficient_material(g->current->pos);
}

bool
game_is_draw_by_50_move_rule(const struct game *g)
{
	return game_half_move_count(g) >= 100;
}

bool
game_is_draw_by_repetition(const struct game *g)
{
	unsigned repetitions = 1;

	const struct history_item *i = g->current;

	while (i->prev != NULL) {
		i = i->prev;
		if (pos_equal(i->pos, g->current->pos))
			++repetitions;
	}

	return repetitions >= 3;
}

struct move
game_get_single_response(const struct game *g)
{
	struct move moves[MOVE_ARRAY_LENGTH];

	(void) gen_moves(g->current->pos, moves);
	return moves[0];
}

bool
game_has_single_response(const struct game *g)
{
	struct move moves[MOVE_ARRAY_LENGTH];

	return gen_moves(g->current->pos, moves) == 1;
}

int
game_reset_fen(struct game *g, const char *fen)
{
	struct game new;

	new.head.next = NULL;
	new.head.prev = NULL;
	new.item_count = 1;
	new.current_index = 0;
	new.current = &g->head;
	new.tail = &g->head;
	new.head.pos = position_allocate();

	if (position_read_fen(fen, new.head.pos) == NULL) {
		position_destroy(new.head.pos);
		return 1;
	}

	g->current_index = 0;
	game_truncate(g);
	history_destroy(g->head.next);
	position_destroy(g->head.pos);

	*g = new;

	return 0;
}

bool
game_continues(const struct game *a, const struct game *b)
{
	const struct history_item *ha = &a->head;
	const struct history_item *hb = &b->head;

	if (game_length(a) < game_length(b))
		return false;

	while (pos_equal(ha->pos, hb->pos) && ha->next != NULL) {
		ha = ha->next;
		hb = hb->next;
	}

	return hb->next == NULL;
}

size_t
game_length(const struct game *g)
{
	return g->item_count;
}

bool
game_has_more_postions(const struct game *g)
{
	return g->current->next != NULL;
}
