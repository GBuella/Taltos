
#include <stdlib.h>

#include "book_types.h"
#include "book.h"
#include "position.h"

#include "builtin_book.inc"

#include "macros.h"

static struct book empty_book = { .type = bt_empty };

struct book *book_open(enum book_type type, const char *path)
{
    switch (type) {
        case bt_polyglot:
            return polyglot_book_open(path);
        case bt_fen:
        case bt_builtin:
            if (type == bt_builtin) {
                return fen_book_parse(builtin_book);
            }
            else {
                return fen_book_open(path);
            }
        case bt_empty:
            return &empty_book;
        default:
            return NULL;
    }
}

static int pick_half_bell_curve(int size)
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
    cut = ((size-1) * 3) / 2;
    result_range_size = ((size-1) * 3) + 1;
    if (n > cut) {
        n -= cut + 1;
    }
    else {
        n = cut - n;
    }
    n = (n * original_size) / (result_range_size / 2);
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
    if (book->type == bt_empty) {
        return NONE_MOVE;
    }
    move moves[16];

    switch (book->type) {
    case bt_polyglot:
        polyglot_book_get_move(book, position, ARRAY_LENGTH(moves), moves);
        break;
    case bt_fen:
        fen_book_get_move(book, position, ARRAY_LENGTH(moves), moves);
        break;
    default:
        unreachable;
    }
    if (moves[0] != 0) {
        return moves[pick_half_bell_curve(mlength(moves))];
    }
    else {
        return NONE_MOVE;
    }
}

void
book_close(struct book *book)
{
    if (book != NULL || book != &empty_book) {
        if (book->file != NULL)
            fclose(book->file);
        free(book);
    }
}

