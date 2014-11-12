
#ifndef BOOK_H
#define BOOK_H

#include "chess.h"

struct book;

enum book_type {
    bt_builtin = 1,
    bt_polyglot = 2,
    bt_fen = 3,
    bt_empty = 4,
};

typedef enum book_type book_type;

struct book *book_open(book_type, const char *path);
move book_get_move(const struct book *, const struct position *);
void book_close(struct book *);

#endif
