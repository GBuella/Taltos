
#ifndef BOOK_H
#define BOOK_H

#include "chess.h"

#include "macros.h"

struct book;

enum book_type {
    bt_builtin = 1,
    bt_polyglot = 2,
    bt_fen = 3,
    bt_empty = 4,
};

struct book *book_open(enum book_type, const char *path)
    attribute_warn_unused_result attribute_nonnull;

move book_get_move(const struct book*, const struct position*)
    attribute_nonnull;

void book_close(struct book*);

#endif
