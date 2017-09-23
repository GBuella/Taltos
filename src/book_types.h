/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#ifndef TALTOS_BOOK_TYPES_H
#define TALTOS_BOOK_TYPES_H

#include <stdio.h>
#include "chess.h"
#include "book.h"

struct book;

struct polyglot_book {
	size_t size;
};

int polyglot_book_open(struct book *book, const char *path);
void polyglot_book_get_move(const struct book*,
				const struct position*,
				size_t msize, move[msize]);
size_t polyglot_book_size(const struct book *book);

struct fen_book {
	size_t count;
	const char **entries;
	char *data;
};

int fen_book_open(struct book *book, const char *path);
void fen_book_close(struct book *book);
size_t fen_book_size(const struct book *book);

void fen_book_get_move(const struct book*,
			const struct position*,
			size_t size,
			move[size]);

struct book {
	enum book_type type;
	FILE *file;
	union {
		struct polyglot_book polyglot_book;
		struct fen_book fen_book;
	};
	char raw[];
};

#endif
