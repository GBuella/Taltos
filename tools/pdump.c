/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */
/*
 * Copyright 2017-2018, Gabor Buella
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

#include "position.h"
#include "str_util.h"
#include "game.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct position *pos;

static void
dump_board(void)
{
	for (int rank = 0; rank <= 7; ++rank) {
		for (int file = 7; file >= 0; --file) {
			char p = pos->board[rank * 8 + file];
			printf("%02hhx ", p);
		}
		printf("   ");
		for (int file = 7; file >= 0; --file) {
			char p = pos->board[rank * 8 + file];
			if (p == 0)
				putchar('.');
			else if (is_valid_piece(p))
				putchar(piece_to_char(p));
			else
				putchar('X');
		}
		putchar('\n');
	}
}

static void
dump_hanging_board(void)
{
	for (int rank = 0; rank <= 7; ++rank) {
		for (int file = 7; file >= 0; --file) {
			char p = pos->hanging[rank * 8 + file];
			printf("%02hhx ", p);
		}
		putchar('\n');
	}
}

static void
print_bitboard_rank(uint64_t bitboard, int r)
{
	for (int f = 7; f >= 0; --f) {
		if (is_nonempty(bitboard & bit64(r * 8 + f)))
			putchar('1');
		else
			putchar('.');
	}
}

static void
dump_bitboards_generic(size_t count, const uint64_t bitboards[count],
		       int index_scale)
{
	for (int r = 0; r < 8; ++r) {
		for (size_t i = 0; i < count; ++ i) {
			if (i > 0)
				fputs("    ", stdout);
			print_bitboard_rank(bitboards[i * index_scale], r);
		}
		putchar('\n');
	}
}

static void
dump_bitboards(size_t count, const uint64_t bitboards[count])
{
	dump_bitboards_generic(count, bitboards, 1);
}

static void
dump_bitboard_pairs(size_t count, const uint64_t bitboards[count * 2])
{
	dump_bitboards_generic(count, bitboards, 2);
	putchar('\n');
	dump_bitboards_generic(count, bitboards + 1, 2);
}

static void
print_piece_enums(void)
{
	for (int i = 2; i < 14; i += 2) {
		switch (i) {
		case pawn:
			printf("pawn        ");
			break;
		case knight:
			printf("knight      ");
			break;
		case bishop:
			printf("bishop      ");
			break;
		case rook:
			printf("rook        ");
			break;
		case queen:
			printf("queen       ");
			break;
		case king:
			printf("king        ");
			break;
		default:
			printf("XXXXXXXX    ");
			break;
		}
	}
}

int
main(int argc, char **argv)
{
	if (argc < 2)
		return EXIT_FAILURE;


	char buf[512];
	buf[0] = '\0';

	++argv;
	while ((*argv != NULL) && (strlen(buf) + strlen(*argv) < sizeof(buf) - 2)) {
		strcat(buf, " ");
		strcat(buf, *(argv++));
	}

	if (*argv != NULL)
		return EXIT_FAILURE;

	struct game *g = game_create_fen(buf);
	if (g == NULL)
		return EXIT_FAILURE;

	pos = game_current_position(g);

	printf("offset 0x%zx: .board\n", offsetof(struct position, board));
	dump_board();

	printf("\noffset 0x%zx: .king_attack_map\n",
	       offsetof(struct position, king_attack_map));
	dump_bitboards(1, &pos->king_attack_map);

	printf("\noffset 0x%zx: .king_danger_map\n",
	       offsetof(struct position, king_danger_map));
	dump_bitboards(1, &pos->king_danger_map);

	printf("\noffset 0x%zx: .ep_index = %" PRIu64 "\n",
	       offsetof(struct position, ep_index), pos->ep_index);

	printf("\noffset 0x%zx: .occupied\n",
	       offsetof(struct position, occupied));
	dump_bitboards(1, &pos->occupied);

	printf("\noffset 0x%zx: .attack\n", offsetof(struct position, attack));
	printf("all         ");
	print_piece_enums();
	putchar('\n');
	dump_bitboard_pairs(7, pos->attack);

	printf("\noffset 0x%zx: .sliding_attacks\n",
	       offsetof(struct position, sliding_attacks));
	dump_bitboards(2, pos->sliding_attacks);

	printf("\noffset 0x%zx: .map\n", offsetof(struct position, map));
	printf("all         ");
	print_piece_enums();
	putchar('\n');
	dump_bitboard_pairs(7, pos->map);

	printf("\noffset 0x%zx: .half_open_files\n",
	       offsetof(struct position, half_open_files));
	dump_bitboard_pairs(1, pos->half_open_files);

	printf("\noffset 0x%zx: .pawn_attack_reach\n",
	       offsetof(struct position, pawn_attack_reach));
	dump_bitboard_pairs(1, pos->pawn_attack_reach);

	printf("\noffset 0x%zx: .rq\n", offsetof(struct position, rq));
	dump_bitboard_pairs(1, pos->rq);

	printf("\noffset 0x%zx: .bq\n", offsetof(struct position, bq));
	dump_bitboard_pairs(1, pos->bq);

	printf("\noffset 0x%zx: .rays[0]\n", offsetof(struct position, rays));
	for (int r = 0; r < 8; ++r) {
		dump_bitboards(8, pos->rays[0] + r * 8);
		putchar('\n');
	}

	printf("\noffset 0x%zx: .rays[1]\n",
	       offsetof(struct position, rays) + sizeof(pos->rays[0]));
	for (int r = 0; r < 8; ++r) {
		dump_bitboards(8, pos->rays[1] + r * 8);
		putchar('\n');
	}

	printf("\noffset 0x%zx: .zhash\n", offsetof(struct position, zhash));
	printf(" 0x%016" PRIx64 "\n\n", pos->zhash);

	printf("\noffset 0x%zx: .cr_white_king_side\n",
	       offsetof(struct position, cr_white_king_side));
	printf(" 0x%02hhx\n\n", pos->cr_white_king_side);

	printf("\noffset 0x%zx: .cr_white_queen_side\n",
	       offsetof(struct position, cr_white_queen_side));
	printf(" 0x%02hhx\n\n", pos->cr_white_queen_side);

	printf("\noffset 0x%zx: .cr_black_king_side\n",
	       offsetof(struct position, cr_black_king_side));
	printf(" 0x%02hhx\n\n", pos->cr_black_king_side);

	printf("\noffset 0x%zx: .cr_black_queen_side\n",
	       offsetof(struct position, cr_black_queen_side));
	printf(" 0x%02hhx\n\n", pos->cr_black_queen_side);

	printf("\noffset 0x%zx: .material_value\n",
	       offsetof(struct position, material_value));
	printf("[white] = %" PRIi32 " [black] = %" PRIi32 "\n\n",
	       pos->material_value[white], pos->material_value[black]);

	printf("\noffset 0x%zx: .king_pins\n",
	       offsetof(struct position, king_pins));
	dump_bitboard_pairs(1, pos->king_pins);

	printf("\noffset 0x%zx: .undefended\n",
	       offsetof(struct position, undefended));
	dump_bitboard_pairs(1, pos->undefended);

	printf("\noffset 0x%zx: .hanging\n",
	       offsetof(struct position, hanging));
	dump_hanging_board();

	printf("\noffset 0x%zx: .hanging_map\n",
	       offsetof(struct position, hanging_map));
	printf(" 0x%016" PRIx64 "\n", pos->hanging_map);

	game_destroy(g);

	return EXIT_SUCCESS;
}
