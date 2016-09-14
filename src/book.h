
/* vim: set filetype=c : */

#ifndef TALTOS_BOOK_H
#define TALTOS_BOOK_H

#include "chess.h"

struct book;

enum book_type
{
	bt_builtin = 1,
	bt_polyglot = 2,
	bt_fen = 3,
	bt_empty = 4,
};

struct book *book_open(enum book_type, const char *path)
	attribute(warn_unused_result);

move book_get_move(const struct book*, const struct position*)
	attribute(nonnull);

void book_close(struct book*);

#endif
