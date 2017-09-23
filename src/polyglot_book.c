/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

/*
 * Code processing the Polyglot chess book format
 *     - loosely based on code from Fabien Letouzey
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "macros.h"
#include "book_types.h"
#include "hash.h"
#include "util.h"

struct entry {
	uint64_t key;
	uint16_t move;
	uint16_t weight;
	uint32_t learn;
};

/* Each entry in a file uses 16 bytes */
enum { file_entry_size = 16 };

int
polyglot_book_open(struct book *book, const char *path)
{
	book->type = bt_polyglot;

	if (path == NULL)
		return -1;

	if ((book->file = fopen(path, "rb")) == NULL)
		return -1;

	size_t file_size;
	if (bin_file_size(book->file, &file_size) != 0)
		return -1;

	if ((file_size % file_entry_size) != 0)
		return -1;

	book->polyglot_book.size = file_size / file_entry_size;

	if (book->polyglot_book.size == 0)
		return -1;

	return 0;
}

static int
get_entry(FILE *f, size_t offset, struct entry *entry)
{
	unsigned char buffer[file_entry_size];

	/*
	 * Seek to the entry in the file, and load it.
	 */
	if (fseek(f, (long)offset * 16, SEEK_SET) != 0)
		return -1;

	if (fread(buffer, sizeof(buffer), 1, f) != 1)
		return -1;

	entry->key = (uint64_t)get_big_endian_num(8, buffer);
	entry->move = (uint16_t)get_big_endian_num(2, buffer + 8);
	entry->weight = (uint16_t)get_big_endian_num(2, buffer + 10);
	entry->learn = (uint32_t)get_big_endian_num(4, buffer + 12);
	return 0;
}

struct search {
	FILE *file;
	size_t size;
	uint64_t key;
	struct entry *entries;
	size_t count;
	size_t max_count;
};

static bool
is_full(const struct search *search)
{
	return search->count >= search->max_count;
}

static void
add_entry(struct search *search, const struct entry *entry)
{
	search->entries[search->count] = *entry;
	search->count++;
}

static int
find_first_entry(FILE *file, uint64_t key, size_t *offset)
{
	/*
	 * There is an entry with the right key at *offset, but it might
	 * not be the first such entry. Look for the first one among the
	 * previous entries.
	 */

	while (*offset > 0) { // Are ther any more keys before this one?
		struct entry entry;
		if (get_entry(file, *offset - 1, &entry) != 0)
			return -1;

		if (entry.key != key) {
			/*
			 * The entry at *offset-1 has a different key, thus
			 * the entry at *offset is the first one with the
			 * right key.
			 */
			return 0;
		}

		--(*offset);
	}

	return 0;
}

static int
load_entries(struct search *search, size_t offset)
{
	if (find_first_entry(search->file, search->key, &offset) != 0)
		return -1;

	while (!is_full(search) && offset < search->size) {
		struct entry entry;

		if (get_entry(search->file, offset, &entry) != 0)
			return -1;

		if (entry.key != search->key)
			return 0;

		if (entry.weight != 0 && entry.move != 0)
			add_entry(search, &entry);

		++offset;
	}

	return 0;
}

static int
get_entries(struct search *search)
{
	size_t low = 0;
	size_t high = search->size;

	if (search->key == UINT64_C(0) || is_full(search))
		return 0;

	while (low < high) {
		long middle = (high + low) / 2;
		struct entry entry;
		if (get_entry(search->file, middle, &entry) != 0)
			return -1;

		if (entry.key == search->key) {
			if (load_entries(search, middle) != 0)
				return -1;
			break;
		}
		else if (entry.key > search->key) {
			high = middle;
		}
		else if (high > low + 1) {
			low = middle;
		}
		else {
			if (get_entry(search->file, high, &entry) != 0)
				return -1;
			if (entry.key == search->key) {
				if (load_entries(search, high) != 0)
					return -1;
			}
			return 0;
		}
	}

	return 0;
}

static int
pmfrom(uint16_t pm)
{
	return ind((pm >> 9) & 7, (pm >> 6) & 7);
}

static int
pmto(uint16_t pm)
{
	return ind((pm >> 3) & 7, pm & 7);
}

static unsigned
pmpromotion(uint16_t pm)
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

static bool
pmove_match(uint16_t polyglot_move, move m, bool flip)
{
	if (flip)
		m = flip_m(m);
	if (pmfrom(polyglot_move) != mfrom(m)
	    || pmto(polyglot_move) != mto(m))
		return false;
	if (is_promotion(m))
		return pmpromotion(polyglot_move) == mresultp(m);
	else
		return pmpromotion(polyglot_move) == 0;
}

static void
pick_legal_moves(struct search *search,
		const struct position *position,
		enum player side, move moves[])
{
	move legal_moves[MOVE_ARRAY_LENGTH];

	(void) gen_moves(position, legal_moves);
	for (unsigned ri = 0; ri < search->count; ++ri) {
		uint16_t m = search->entries[ri].move;

		for (unsigned i = 0; legal_moves[i] != 0; ++i) {
			if (pmove_match(m, legal_moves[i], side == white)) {
				*moves++ = legal_moves[i];
				break;
			}
		}
	}
	*moves = 0;
}

static int
cmp(const void *a, const void *b)
{
	uint16_t aw = ((const struct entry*)a)->weight;
	uint16_t bw = ((const struct entry*)b)->weight;

	if (aw < bw)
		return 1;
	else if (aw > bw)
		return -1;
	else
		return 0;
}

static void
sort_entries(size_t count, struct entry entries[count])
{
	qsort(entries, count, sizeof(entries[0]), cmp);
}

void
polyglot_book_get_move(const struct book *book,
			const struct position *position,
			size_t msize, move moves[msize])
{

	moves[0] = 0;
	if (msize < 2)
		return;

	struct entry entries[msize - 1];
	struct search search = {.entries = entries,
				.count = 0,
				.max_count = msize - 1,
				.file = book->file,
				.size = book->polyglot_book.size, };

	enum player side;

	side = white;
	search.key = position_polyglot_key(position, side);
	if (get_entries(&search) != 0)
		search.count = 0;

	if (search.count == 0) {
		side = black;
		search.key = position_polyglot_key(position, side);
		if (get_entries(&search) != 0) {
			search.count = 0;
		}

		if (search.count == 0)
			return;
	}

	sort_entries(search.count, entries);

	pick_legal_moves(&search, position, side, moves);
}

size_t
polyglot_book_size(const struct book *book)
{
	return book->polyglot_book.size;
}
