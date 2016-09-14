
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "chess.h"
#include "str_util.h"
#include "util.h"
#include "book_types.h"

static void
validate_fen_book_entry(const char *line_start, jmp_buf jb)
{
	const char *str;

	str = position_read_fen(NULL, line_start, NULL, NULL);
	if (str == NULL) longjmp(jb, 1);
	while ((str = next_token(str)) != NULL) {
		if (fen_read_move(line_start, str, NULL) != 0)
			longjmp(jb, 1);
	}
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

static int
parse_raw(char *raw, struct fen_book *book)
{
	book->count = 0;
	raw += strspn(raw, "\n\r");
	while (*raw != '\0') {

		if (book->count == ARRAY_LENGTH(book->entries))
			return -1;

		jmp_buf jb;
		if (setjmp(jb) != 0)
			return -1;

		book->entries[book->count] = raw;

		raw += strcspn(raw, "\n\r");
		if (*raw != '\0') {
			*raw++ = '\0';
			raw += strspn(raw, "\n\r");
		}
		if (book->entries[book->count][0] != '#') {
			validate_fen_book_entry(book->entries[book->count], jb);
			++book->count;
		}
	}
	qsort(book->entries, book->count, sizeof(book->entries[0]), cmp_entry);
	return 0;
}

struct book
*fen_book_open(const char *path)
{
	FILE *f = NULL;
	struct book *book = NULL;
	size_t file_size;

	if ((f = fopen(path, "r")) == NULL)
		return NULL;

	if (bin_file_size(f, &file_size) != 0)
		goto error;

	book = xmalloc(sizeof *book + file_size + 1);
	book->type = bt_fen;
	book->file = NULL;

	if (fread(book->raw, 1, file_size, f) != 1)
		goto error;

	book->raw[file_size] = '\0';

	if (parse_raw(book->raw, &book->fen_book) != 0)
		goto error;

	if (ferror(f))
		goto error;

	(void) fclose(f);
	return book;

error:
	if (book != NULL)
		free(book);
	if (f != NULL)
		fclose(f);
	return NULL;
}

struct book*
fen_book_parse(const char * const * volatile raw)
{
	if (raw == NULL)
		return NULL;

	struct book *book = xcalloc(1, sizeof(*book));
	jmp_buf jb;

	book->type = bt_fen;
	if (setjmp(jb) != 0) {
		free(book);
		return NULL;
	}
	do {
		if (book->fen_book.count
		    == ARRAY_LENGTH(book->fen_book.entries))
			longjmp(jb, 1);
		validate_fen_book_entry(*raw, jb);
		book->fen_book.entries[book->fen_book.count++] = *raw++;
	} while (*raw != NULL);
	qsort(book->fen_book.entries, book->fen_book.count,
	    sizeof book->fen_book.entries[0], cmp_entry);
	return book;
}

void
fen_book_get_move(const struct book *book,
		const struct position *position,
		size_t size,
		move m[size])
{
	char fen[FEN_BUFFER_LENGTH];
	const char **entry;
	const char *str;

	m[0] = 0;
	if (book == NULL || position == NULL) {
		return;
	}
	(void) position_print_fen(position, fen, 0, white);
	entry = bsearch(fen, book->fen_book.entries,
	    book->fen_book.count,
	    sizeof book->fen_book.entries[0],
	    cmp_entry_key);
	if (entry == NULL) {
		(void) position_print_fen(position, fen, 0, black);
		entry = bsearch(fen, book->fen_book.entries,
		    book->fen_book.count,
		    sizeof book->fen_book.entries[0],
		    cmp_entry_key);
		if (entry == NULL) {
			return;
		}
	}
	str = position_read_fen(NULL, *entry, NULL, NULL);
	while ((size > 0) && ((str = next_token(str)) != NULL)) {
		fen_read_move(*entry, str, m);
		++m;
		--size;
	}
	*m = 0;
}
