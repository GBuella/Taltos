
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "chess.h"
#include "str_util.h"
#include "fen_book.h"

struct fen_book {
    size_t size;
    char *entries[8192];
};

static void validate_fen_book_entry(const char *line_start, jmp_buf jb)
{
    const char *str;
    
    str = position_read_fen(NULL, line_start, NULL);
    if (str == NULL) longjmp(jb, 1);
    while ((str = next_token(str)) != NULL) {
        if (fen_read_move(line_start, str, NULL) != 0) longjmp(jb, 1);
    }
}

static void
fen_book_add_line(struct fen_book *book, const char *str, jmp_buf jb)
{
    char *entry;

    if (book->size >= ARRAY_LENGTH(book->entries)) longjmp(jb, 1);
    validate_fen_book_entry(str, jb);
    if ((entry = malloc(strlen(str) + 1)) == NULL) longjmp(jb, 1);
    strcpy(entry, str);
    book->entries[book->size] = entry;
    book->size++;
    book->entries[book->size] = NULL;
}

static int cmp_entry(const void *a, const void *b)
{
    return strcmp(*((const char **)a), *((const char **)b));
}

static int cmp_entry_key(const void *a, const void *b)
{
    return strncmp(a, *((const char **)b), strlen(a));
}

struct fen_book *fen_book_open(const char *path)
{
    struct fen_book *book = malloc(sizeof *book);
    char line[256];
    FILE *f = fopen(path, "r");
    jmp_buf jb;

    if (book == NULL || f == NULL) goto error;
    if (setjmp(jb) != 0) goto error;
    book->entries[0] = NULL;
    book->size = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strlen(line) > sizeof line - 2) goto error;
        if (!empty_line(line)) {
            fen_book_add_line(book, line, jb);
        }
    }
    qsort(book->entries, book->size, sizeof book->entries[0], cmp_entry);
    if (ferror(f)) goto error;
    fclose(f);
    return book;

error:
    fen_book_close(book);
    if (f != NULL) fclose(f);
    return NULL;
}

struct fen_book *fen_book_parse(const char * const *data)
{
    if (data == NULL) return NULL;

    struct fen_book *book = malloc(sizeof *book);
    jmp_buf jb;

    if (book == NULL) return NULL;
    if (setjmp(jb) != 0) {
        fen_book_close(book);
        return NULL;
    }
    book->entries[0] = NULL;
    while (*data != NULL) {
        if (!empty_line(*data)) {
            fen_book_add_line(book, *data, jb);
        }
        ++data;
    }
    qsort(book->entries, book->size, sizeof book->entries[0], cmp_entry);
    return book;
}

void
fen_book_get_move(const struct fen_book *book,
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
    (void)position_print_fen(position, fen, white);
    entry = bsearch(fen, book->entries,
                    book->size, sizeof book->entries[0],
                    cmp_entry_key);
    if (entry == NULL) {
        (void)position_print_fen(position, fen, black);
        entry = bsearch(fen, book->entries,
                        book->size, sizeof book->entries[0],
                        cmp_entry_key);
        if (entry == NULL) {
            return;
        }
    }
    str = position_read_fen(NULL, *entry, NULL);
    while ((size > 0) && ((str = next_token(str)) != NULL)) {
        fen_read_move(*entry, str, m);
        ++m;
        --size;
    }
    *m = 0;
}

void fen_book_close(struct fen_book *book)
{
    if (book != NULL) {
        for (char **entry = &(book->entries[0]); *entry != NULL; ++entry) {
            free(*entry);
        }
        free(book);
    }
}

