
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

struct book *polyglot_book_open(const char *path);
void polyglot_book_get_move(const struct book*,
				const struct position*,
				size_t size,
				move[size]);


struct fen_book {
	size_t count;
	const char *entries[0x10000];
};

struct book *fen_book_open(const char *path);
struct book *fen_book_parse(const char * const * volatile data);

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
