/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
	pawn,
	opp_pawn,
	nb, // knight or bishop
	opp_nb, // knight or bishop
	rook,
	opp_rook,
	queen,
	opp_queen,
	king,
	opp_king,
	piece_array_size
};

static const uint8_t piece_value[piece_array_size] = {
	[pawn] = 1,
	[nb] = 3,
	[rook] = 5,
	[queen] = 9,
	[opp_pawn] = 1,
	[opp_nb] = 3,
	[opp_rook] = 5,
	[opp_queen] = 9
};

enum {
	code_per_size  = 2 * 2 * 3 * 4 * 3,
	max_code = code_per_size * code_per_size - 1
};

int table[max_code + 1];

static void
unpack_code(unsigned code, unsigned attackers[piece_array_size])
{
	attackers[pawn] = code % 3;
	code /= 3;

	attackers[nb] = code % 4;
	code /= 4;

	attackers[rook] = code % 3;
	code /= 3;

	attackers[queen] = code % 2;
	code /= 2;

	attackers[king] = code % 2;
	code /= 2;

	attackers[opp_pawn] = code % 3;
	code /= 3;

	attackers[opp_nb] = code % 4;
	code /= 4;

	attackers[opp_rook] = code % 3;
	code /= 3;

	attackers[opp_queen] = code % 2;
	code /= 2;

	attackers[opp_king] = code % 2;
}

static bool
can_king_capture(unsigned attackers[piece_array_size], unsigned side)
{
	if (attackers[king + side] == 0)
		return false;

	for (unsigned i = side ^ 1; i < piece_array_size; i += 2) {
		if (attackers[i] != 0)
			return false;
	}

	return true;
}

static int
SEE_negamax(unsigned attackers[piece_array_size], unsigned piece,
	    int value, unsigned side)
{
	unsigned next_piece = 0;
	if (attackers[pawn + side] > 0)
		next_piece = pawn;
	else if (attackers[nb + side] > 0)
		next_piece = nb;
	else if (attackers[rook + side] > 0)
		next_piece = rook;
	else if (attackers[queen + side] > 0)
		next_piece = queen;
	else if (can_king_capture(attackers, side))
		next_piece = king;
	else
		return value;

	attackers[next_piece + side]--;
	int capture_value = value + piece_value[piece];
	capture_value = -SEE_negamax(attackers, next_piece, -capture_value,
				     side ^ 1);

	if (capture_value > value)
		return capture_value;
	else
		return value;
}

static void
gen_table(void)
{
	for (unsigned i = 0; i <= max_code; ++i) {
		unsigned attackers[piece_array_size];

		unpack_code(i, attackers);
		table[i] = 9 - SEE_negamax(attackers, queen, 0, 1);
		if (table[i] == 9)
			table[i] = 10;
	}
}

static void
print_table(void)
{
	printf("const uint8_t SEE_values[%d] = {\n", max_code + 1);
	for (unsigned i = 0; i <= max_code; ++i) {
		if (i % 16 == 0)
			putchar('\t');
		if (table[i] < 10)
			putchar(' ');
		printf("%d", table[i]);
		if (i < max_code)
			putchar(',');
		if (i % 16 == 15 || i == max_code)
			putchar('\n');
		else
			putchar(' ');
	}
	puts("};");
}

static void
print_prologue(const char *guard)
{
	puts("/* Lookup tables used to compute SEE values in Taltos */");
	puts("/* Generated file, do not edit manually */");
	puts("");
	printf("#ifndef %s\n", guard);
	printf("#define %s\n", guard);
	puts("");
	puts("#include <stdint.h>");
}

static void
print_epilogue(void)
{
	puts("#endif");
}

int
main(int argc, char **argv)
{
	const char *guard = "TALTOS_SEE_INC";

	if (argc > 3)
		return EXIT_FAILURE;

	if (argc > 1) {
		if (freopen(argv[1], "w", stdout) == NULL) {
			perror(argv[1]);
			return EXIT_FAILURE;
		}
	}

	if (argc > 2)
		guard = argv[2];

	gen_table();

	print_prologue(guard);
	putchar('\n');
	print_table();
	putchar('\n');
	print_epilogue();

	return EXIT_SUCCESS;
}
