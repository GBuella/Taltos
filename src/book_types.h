
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

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
				size_t size,
				move[size]);


struct fen_book {
	size_t count;
	const char **entries;
	char *data;
};

int fen_book_open(struct book *book, const char *path);
int fen_book_parse(struct book *book, const char *data);
void fen_book_close(struct book *book);

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
