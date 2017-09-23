/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_BOOK_H
#define TALTOS_BOOK_H

#include "chess.h"

struct book;

enum book_type
{
	bt_polyglot = 2,
	bt_fen = 3,
	bt_empty = 4,
};

struct book *book_open(enum book_type, const char *path)
	attribute(warn_unused_result);

void book_get_move_list(const struct book*, const struct position*,
		move moves[static MOVE_ARRAY_LENGTH])
	attribute(nonnull);

move book_get_move(const struct book*, const struct position*)
	attribute(nonnull);

size_t book_get_size(const struct book*);

void book_close(struct book*);

#endif
