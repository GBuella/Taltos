/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
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

#include <stdlib.h>

#include "book_types.h"
#include "book.h"

#include "macros.h"

static struct book empty_book = { .type = bt_empty };

struct book*
book_open(enum book_type type, const char *path)
{
	if (type == bt_empty)
		return &empty_book;

	struct book *book = calloc(1, sizeof(*book));
	if (book == NULL)
		return NULL;

	switch (type) {
		case bt_polyglot:
			if (polyglot_book_open(book, path) == 0)
				return book;
			break;
		case bt_fen:
			if (fen_book_open(book, path) == 0)
				return book;
			break;
		default:
			break;
	}

	book_close(book);
	return NULL;
}

static int
pick_half_bell_curve(int size)
{
	assert(size > 0 && size < 1024);

	if (size < 2) {
		return 0;
	}
	int n = rand();
	int cut;
	int original_size = size;
	int result_range_size;

	if ((size % 2) == 1) {
		++size;
	}
	n = (n % size) + ((n >> 10) % size) + ((n >> 20) % size);
	cut = ((size - 1) * 3) / 2;
	result_range_size = ((size - 1) * 3) + 1;
	if (n > cut) {
		n -= cut + 1;
	}
	else {
		n = cut - n;
	}
	n = (n * original_size) / (result_range_size / 2);
	return n;
}

static int
mlength(const move *m)
{
	int l = 0;
	while (*m != 0) {
		++l;
		++m;
	}
	return l;
}

void
book_get_move_list(const struct book *book, const struct position *position,
		move moves[static MOVE_ARRAY_LENGTH])
{
	if (book->type == bt_empty) {
		moves[0] = 0;
		return;
	}

	switch (book->type) {
	case bt_polyglot:
		polyglot_book_get_move(book, position,
		    MOVE_ARRAY_LENGTH, moves);
		break;
	case bt_fen:
		fen_book_get_move(book, position,
		    MOVE_ARRAY_LENGTH, moves);
		break;
	default:
		unreachable;
	}

	return;
}

move
book_get_move(const struct book *book, const struct position *position)
{
	if (book->type == bt_empty)
		return none_move;
	move moves[MOVE_ARRAY_LENGTH];

	book_get_move_list(book, position, moves);

	if (moves[0] != 0)
		return moves[pick_half_bell_curve(mlength(moves))];
	else
		return none_move;
}

size_t
book_get_size(const struct book *book)
{
	switch (book->type) {
	case bt_polyglot:
		return polyglot_book_size(book);
	case bt_fen:
		return fen_book_size(book);
	default:
		return 0;
	}
}

void
book_close(struct book *book)
{
	if (book != NULL && book != &empty_book) {
		if (book->file != NULL)
			fclose(book->file);
		switch (book->type) {
		case bt_fen:
			fen_book_close(book);
			break;
		default:
			break;
		}
		free(book);
	}
}
