/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=(0,t0: */
/*
 * Copyright 2017, Gabor Buella
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

#include "bitboard.h"

#include <cstdio>

int
main(int argc, char **argv)
{
	using namespace taltos;

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
