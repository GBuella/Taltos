
/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=(0,t0: */

#include "bitboard.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
	(void) argc;
	++argv;

	for (; *argv != NULL; ++argv) {
		puts("-----------------------");
		char *endptr;
		uint64_t n = (uint64_t)strtoumax(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Invalid number %s\n", *argv);
			return EXIT_FAILURE;
		}

		printf("hex: 0x%016" PRIX64 "\n", n);
		printf("dec: %" PRIu64 "\n", n);
		uint64_t swapped = bswap(n);
		puts("             flipped");
		puts("  ABCDEFGH   ABCDEFGH");
		for (int r = 0; r < 8; ++r) {
			printf("%d ", 8 - r);
			for (int f = 7; f >= 0; --f) {
				if (is_nonempty(n & bit64(r * 8 + f)))
					putchar('1');
				else
					putchar('.');
			}
			printf(" %d ", 8 - r);
			for (int f = 7; f >= 0; --f) {
				if (is_nonempty(swapped & bit64(r * 8 + f)))
					putchar('1');
				else
					putchar('.');
			}
			printf(" %d\n", 8 - r);
		}
		puts("  ABCDEFGH   ABCDEFGH");
	}

	return EXIT_SUCCESS;
}
