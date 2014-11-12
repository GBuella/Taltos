
/* Code processing the Polyglot chess book format
       - based on code from Fabien Letouzey
         available under the GNU General Public License.
*/

#include <stdbool.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include "macros.h"
#include "polyglotbook.h"
#include "hash.h"
#include "platform.h"

struct entry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};

typedef struct entry entry;

bool has_key(entry entry, uint64_t key)
{
    return entry.key == key;
}

struct polyglotbook {
    FILE *f;
    long size;
};

struct polyglotbook *polyglotbook_open(const char *path)
{
    struct polyglotbook *book;
    long filesize;

    if (path == NULL) return NULL;
    book = malloc(sizeof *book);
    if (book == NULL) return NULL;
    book->f = fopen(path, "rb");
    if (book->f == 0 || fseek(book->f, 0, SEEK_END) != 0) {
        polyglotbook_close(book);
        return NULL;
    }
    filesize = ftell(book->f);
    if (filesize < 16 || filesize % 16 != 0) {
        polyglotbook_close(book);
        return NULL;
    }
    book->size = filesize / 16;
    return book;
}

void polyglotbook_close(struct polyglotbook *book)
{
    if (book == NULL) return;
    if (book->f != NULL) {
        fclose(book->f);
    }
    free(book);
}

static entry get_entry(FILE *f, long offset, jmp_buf jb)
{
    entry entry;
    unsigned char buffer[16];

    if (fseek(f, offset*16, SEEK_SET) != 0) longjmp(jb, 1);
    if (fread(buffer, 1, 16, f) != 16) longjmp(jb, 1);
    entry.key = (uint64_t)get_big_endian_num(8, buffer);
    entry.move = (uint16_t)get_big_endian_num(2, buffer + 8);
    entry.weight = (uint16_t)get_big_endian_num(2, buffer + 10);
    entry.learn = (uint32_t)get_big_endian_num(4, buffer + 12);
    return entry;
}

struct result_set {
    entry *entries;
    size_t count;
    size_t max_count;
};

bool is_full(const struct result_set *r)
{
    return r->count >= r->max_count;
}

void add_entry(struct result_set *r, entry entry)
{
    r->entries[r->count] = entry;
    r->count++;
}

static long
find_first_entry(FILE *f, uint64_t key, long offset, jmp_buf jb)
{
    while (offset > 0 && has_key(get_entry(f, offset-1, jb), key)) {
        --offset;
    }
    return offset;
}

static void
load_entries(const struct polyglotbook *book,
             uint64_t key,
             struct result_set *results,
             long offset,
             jmp_buf jb)
{
    offset = find_first_entry(book->f, key, offset, jb);
    while (!is_full(results) && offset < book->size) {
        entry entry = get_entry(book->f, offset, jb);

        if (entry.key != key) break;
        if (entry.weight != 0 && entry.move != 0) {
            add_entry(results, entry);
        }
        ++offset;
    }
}

static void
get_entries(const struct polyglotbook *book,
            uint64_t key,
            struct result_set *results,
            jmp_buf jb)
{
    long low = 0;
    long high = book->size;

    if (key == UINT64_C(0) || is_full(results)) {
        return;
    }
    while (low < high) {
        long middle = (high + low) / 2;
        entry entry = get_entry(book->f, middle, jb);

        if (entry.key == key) {
            load_entries(book, key, results, middle, jb);
            break;
        }
        else if (entry.key > key) {
            high = middle;
        }
        else if (high > low + 1) {
            low = middle;
        }
        else {
            entry = get_entry(book->f, high, jb);
            if (entry.key == key) {
                load_entries(book, key, results, high, jb);
            }
            break;
        }
    }
}

static int pmfrom(uint16_t pm)
{
    return ind((pm >> 9) & 7, (pm >> 6) & 7);
}

static int pmto(uint16_t pm)
{
    return ind((pm >> 3) & 7, pm & 7);
}

static int pmpromotion(uint16_t pm)
{
    switch (pm >> 12) {
    case 1:
        return knight;
    case 2:
        return bishop;
    case 3:
        return rook;
    case 4:
        return queen;
    default:
        return 0;
    }
}

static bool pmove_match(uint16_t polyglot_move, move m, bool flip)
{
    if (flip) {
        m = flip_m(m);
    }
    return pmfrom(polyglot_move) == mfrom(m)
           && pmto(polyglot_move) == mto(m)
           && (pmpromotion(polyglot_move)
               == (is_promotion(m) ? mpromotion(m) : 0));
}

static void pick_legal_moves(struct result_set *results,
                             const struct position *position,
                             player side,
                             move moves[])
{
    move legal_moves[MOVE_ARRAY_LENGTH];

    (void)gen_moves(position, legal_moves);
    for (unsigned ri = 0; ri < results->count; ++ri) {
        uint16_t m = results->entries[ri].move;

        for (unsigned i = 0; legal_moves[i] != 0; ++i) {
            if (pmove_match(m, legal_moves[i], side == white)) {
                *moves++ = legal_moves[i];
                break;
            }
        }
    }
    *moves++ = 0;
}

static int cmp(const void *a, const void *b)
{
    uint16_t aw = ((const entry *)a)->weight;
    uint16_t bw = ((const entry *)b)->weight;
    
    if (aw > bw) {
        return 1;
    }
    else if (aw < bw) {
        return -1;
    }
    else {
        return 0;
    }
}

void polyglotbook_get_move(const struct polyglotbook *book,
                           const struct position *position,
                           size_t msize,
                           move moves[msize])
{
    struct result_set results[1];
    jmp_buf jb;
    player side;

    moves[0] = 0;
    if (book == NULL || position == NULL || msize < 2) {
        return;
    }

    entry entries[msize-1];

    results->entries = entries;
    results->count = 0;
    results->max_count = msize - 1;
    if (setjmp(jb) != 0) {
        return;
    }
    get_entries(book, position_polyglot_key(position, white), results, jb);
    if (results->count > 0) {
        side = white;
    }
    else {
        get_entries(book, position_polyglot_key(position, black), results, jb);
        if (results->count > 0) {
            side = black;
        }
        else {
            return;
        }
    }
    qsort(results->entries, results->count, sizeof(results->entries[0]), cmp);
    pick_legal_moves(results, position, side, moves);
}

