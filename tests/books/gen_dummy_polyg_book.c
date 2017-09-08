
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

/*
 * Generating a dummy opening book, for testing.
 * Values are from: http://hgm.nubati.net/book_format.html
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *f;

static void
dump_big_endian(size_t size, uint64_t number)
{
	while (size > 0) {
		int c = (int)((number >> (size - 1) * 8) % 0x100);
		if (fputc(c, f) == EOF) {
			perror("writing output");
			exit(EXIT_FAILURE);
		}
		--size;
	}
}

struct entry {
	uint64_t key;
	uint16_t move;
	uint16_t weight;
	uint32_t learn;
};

#define PMOVE(from_file, from_row, to_file, to_row, promotion) \
	(to_file + (to_row << 3) + (from_file << 6) + (from_row << 9) + \
	(promotion << 12))

static void
dump_entry(const struct entry *entry)
{
	dump_big_endian(sizeof(entry->key), entry->key);
	dump_big_endian(sizeof(entry->move), entry->move);
	dump_big_endian(sizeof(entry->weight), entry->weight);
	dump_big_endian(sizeof(entry->learn), entry->learn);
}

enum polyglot_file {
	fH,
	fG,
	fF,
	fE,
	fD,
	fC,
	fB,
	fA
};

enum polyglot_row {
	r1,
	r2,
	r3,
	r4,
	r5,
	r6,
	r7
};

struct entry entries[] = {
	{ .key = UINT64_C(0x463b96181691fc9c),
		.move = PMOVE(fE, r2, fE, r4, 0),
		.weight = 1, },
	{ .key = UINT64_C(0x823c9b50fd114196),
		.move = PMOVE(fE, r7, fE, r5, 0),
		.weight = 2, },
	{ .key = UINT64_C(0x823c9b50fd114196),
		.move = PMOVE(fH, r7, fH, r6, 0),
		.weight = 1, },
	{ .key = UINT64_C(0x0756b94461c50fb0),
		.move = PMOVE(fB, r1, fC, r3, 0),
		.weight = 2, }
};

static int
cmp_entry(const void *a, const void *b)
{
	uint64_t key_a = ((const struct entry *)a)->key;
	uint64_t key_b = ((const struct entry *)b)->key;

	if (key_a < key_b)
		return -1;
	else if (key_a == key_b)
		return 0;
	else
		return 1;
}

int
main(int argc, char **argv)
{
	if (argc < 3)
		return EXIT_FAILURE;

	size_t count = sizeof(entries) / sizeof(entries[0]);
	qsort(entries, count, sizeof(entries[0]), cmp_entry);

	if ((f = fopen(argv[1], "w")) == NULL) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	for (size_t i = 0; i < count; ++i)
		dump_entry(entries + i);

	fclose(f);

	if ((f = fopen(argv[2], "w")) == NULL) {
		perror(argv[2]);
		return EXIT_FAILURE;
	}

	dump_entry(entries);

	fclose(f);

	return EXIT_SUCCESS;
}
