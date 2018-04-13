/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2014-2017, Gabor Buella
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
				size_t msize, struct move[msize]);
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
			struct move[size]);

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
