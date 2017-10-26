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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "chess.h"
#include "str_util.h"
#include "util.h"
#include "book_types.h"

static int
validate_fen_book_entry(const char *line_start)
{
	const char *str;

	str = position_read_fen(NULL, line_start, NULL, NULL);
	if (str == NULL)
		return -1;

	while ((str = next_token(str)) != NULL) {
		if (fen_read_move(line_start, str, NULL) != 0)
			return -1;
	}
	return 0;
}

static int
cmp_entry(const void *a, const void *b)
{
	return strcmp(*((const char **)a), *((const char **)b));
}

static int
cmp_entry_key(const void *a, const void *b)
{
	return strncmp(a, *((const char **)b), strlen(a));
}

static void
sort_entries(struct fen_book *book)
{
	qsort(book->entries, book->count, sizeof(book->entries[0]), cmp_entry);
}

static const char*
lookup_entry(const struct fen_book *book, const char *fen)
{
	const char **entry = bsearch(fen, book->entries, book->count,
	    sizeof(book->entries[0]), cmp_entry_key);

	if (entry != NULL)
		return *entry;
	else
		return NULL;
}

static int
allocate_entries(struct fen_book *book, size_t *allocated)
{
	if (book->count * sizeof(book->entries[0]) < *allocated)
		return 0;

	if (*allocated == 0)
		*allocated = sizeof(book->entries[0]);
	else
		*allocated *= 2;

	const char **new = realloc(book->entries, *allocated);
	if (new == NULL)
		return -1;

	book->entries = new;
	return 0;
}

static int
parse_raw(struct fen_book *book)
{
	size_t allocated = 0;
	book->count = 0;
	char *raw = book->data;
	raw += strspn(raw, "\n\r");

	while (*raw != '\0') {
		if (allocate_entries(book, &allocated) != 0)
			return -1;

		const char *line = raw;

		raw += strcspn(raw, "\n\r");
		if (*raw != '\0') {
			*raw++ = '\0';
			raw += strspn(raw, "\n\r");
		}
		if (line[0] != '#') {
			if (validate_fen_book_entry(line) != 0)
				return -1;
			book->entries[book->count] = line;
			book->count++;
		}
	}

	sort_entries(book);
	return 0;
}

int
fen_book_read_file(struct fen_book *book, FILE *f)
{
	size_t file_size;

	if (bin_file_size(f, &file_size) != 0)
		return -1;

	if ((book->data = malloc(file_size + 1)) == NULL)
		return -1;

	if (fread(book->data, file_size, 1, f) != 1)
		return -1;

	book->data[file_size] = '\0';

	return parse_raw(book);
}

int
fen_book_open(struct book *book, const char *path)
{
	FILE *f;

	book->type = bt_fen;

	if ((f = fopen(path, "r")) == NULL)
		return -1;

	if (fen_book_read_file(&book->fen_book, f) != 0) {
		fclose(f);
		return -1;
	}

	return 0;
}

void
fen_book_get_move(const struct book *book,
		const struct position *position,
		size_t size,
		move m[size])
{
	char fen[FEN_BUFFER_LENGTH];
	const char *entry;
	const char *str;

	m[0] = 0;
	if (book == NULL || position == NULL)
		return;

	(void) position_print_fen(position, fen, 0, white);
	entry = lookup_entry(&book->fen_book, fen);
	if (entry == NULL) {
		(void) position_print_fen(position, fen, 0, black);
		entry = lookup_entry(&book->fen_book, fen);
	}
	if (entry == NULL)
		return;

	str = position_read_fen(NULL, entry, NULL, NULL);
	while ((size > 0) && ((str = next_token(str)) != NULL)) {
		fen_read_move(entry, str, m);
		++m;
		--size;
	}
	*m = 0;
}

size_t
fen_book_size(const struct book *book)
{
	return book->fen_book.count;
}

void
fen_book_close(struct book *book)
{
	free(book->fen_book.entries);
	free(book->fen_book.data);
}
