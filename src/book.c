
#include <stdlib.h>

#include "polyglotbook.h"
#include "fen_book.h"
#include "book.h"
#include "position.h"

#include "builtin_book.inc"

typedef void (*t_fn_get_move)(const void *,
                              const struct position *,
                              size_t size,
                              move[size]);

struct book {
    book_type type;
    void *internal_book;
    t_fn_get_move fn_get_move;
    void (*fn_close)(const void *);
};

static void nop() {}

static void
constant_NONE_MOVE(const void *ignored,
                   const struct position *ignoredpos,
                   size_t ignored_size,
                   move moves[ignored_size])
{
    (void)ignored;
    (void)ignoredpos;
    moves[0] = 0;
}

struct book *book_open(book_type type, const char *path)
{
    struct book *book= malloc(sizeof *book);

    if (book == NULL) return NULL;
    switch (type) {
    case bt_polyglot:
        book->internal_book = polyglotbook_open(path);
        if (book->internal_book == NULL) {
            free(book);
            return NULL;
        }
        book->fn_get_move = (t_fn_get_move)polyglotbook_get_move;
        book->fn_close = (void (*)(const void *))polyglotbook_close;
        break;
    case bt_fen:
    case bt_builtin:
        if (type == bt_builtin) {
            book->internal_book = fen_book_parse(builtin_book);
        }
        else {
            book->internal_book = fen_book_open(path);
        }
        if (book->internal_book == NULL) {
            free(book);
            return NULL;
        }
        book->fn_get_move = (t_fn_get_move)fen_book_get_move;
        book->fn_close = (void (*)(const void *))fen_book_close;
        break;
    case bt_empty:
        book->internal_book = NULL;
        book->fn_get_move = constant_NONE_MOVE;
        book->fn_close = nop;
        break;
    default:
        free(book);
        return NULL;
    }
    return book;
}

static int pick_half_bell_curve(int size)
{
    if (size < 2) {
        return 0;
    }
    int n = rand();

    n = 1 + (n % size) + ((n >> 8) % size) + 
        ((n >> 16) % size) + ((n >> 24) % size);
    --size;
    if (n > size*2) {
        n -= size*2 + 1;
    }
    else {
        n = size*2 - n;
    }
    return n;
}

static int mlength(const move *m)
{
    int l = 0;
    while (*m != 0) {
        ++l;
        ++m;
    }
    return l;
}

move
book_get_move(const struct book *book, const struct position *position)
{
    if (book == NULL || position == NULL) {
        return NONE_MOVE;
    }
    move moves[16];

    book->fn_get_move(book->internal_book,
                      position,
                      ARRAY_LENGTH(moves),
                      moves);
    if (moves[0] != 0) {
        return moves[pick_half_bell_curve(mlength(moves))];
    }
    else {
        return NONE_MOVE;
    }
}

void book_close(struct book *book)
{
    if (book == NULL) {
        return;
    }
    book->fn_close(book->internal_book);
    free(book);
}

