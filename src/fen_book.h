
#ifndef FEN_BOOK
#define FEN_BOOK

#include "chess.h"

struct fen_book;

struct fen_book *fen_book_open(const char *path);
struct fen_book *fen_book_parse(const char * const *data);

void fen_book_get_move(const struct fen_book *,
                       const struct position *,
                       size_t size,
                       move[size]);

void fen_book_close(struct fen_book *);

#endif
