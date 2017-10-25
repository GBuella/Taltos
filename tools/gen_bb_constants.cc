/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#include "bitboard.h"
#include "index.h"

#include <cstdio>
#include <cstdlib>

using namespace taltos;

static bitboard l_knight_pattern[64];
static bitboard l_king_pattern[64];
static bitboard l_diag_masks[64];
static bitboard l_adiag_masks[64];
static bitboard l_hor_masks[64];
static bitboard l_ver_masks[64];
static bitboard l_bishop_masks[64];
static bitboard l_rook_masks[64];
static bitboard l_pawn_attacks_north[64];
static bitboard l_pawn_attacks_south[64];

static const int king_dirs_h[] = { 1, 1, 1, 0, -1, -1, -1, 0};
static const int king_dirs_v[] = { -1, 0, 1, 1, 1, 0, -1, -1};

static const int knight_dirs_h[] = { -2, -1, -2, -1,  2,  1,  2,  1};
static const int knight_dirs_v[] = { -1, -2,  1,  2, -1, -2,  1,  2};

static const int rook_dirs[4] = {east, west, north, south};

static const bitboard rook_edges[4] = { bb_file_h, bb_file_a, bb_rank_8, bb_rank_1};

static const int bishop_dirs[4] = {
	east + south,
	west + north,
	west + south,
	east + north};

static const bitboard bishop_edges[4] = {
	bb_file_h | bb_rank_1,
	bb_file_a | bb_rank_8,
	bb_file_a | bb_rank_1,
	bb_file_h | bb_rank_8};

static bitboard pawn_reach_south(bitboard map)
{
	return east_of(south_of(map & ~bb_file_h)) | west_of(south_of(map & ~bb_file_a));
}

static bitboard pawn_reach_north(bitboard map)
{
	return east_of(north_of(map & ~bb_file_h)) | west_of(north_of(map & ~bb_file_a));
}

static void
gen_simple_table(bitboard table[64], const int dirs_v[8], const int dirs_h[8])
{
	for (int rank = 0; rank < 8; ++rank) {
		for (int file = 0; file < 8; ++file) {
			for (int dir_i = 0; dir_i < 8; ++dir_i) {
				int r = rank + dirs_v[dir_i];
				int f = file + dirs_h[dir_i];
				if (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
					int i = ind(r, f);
					table[ind(rank, file)] |= bb(i);
				}
			}
		}
	}
}

static bitboard gen_ray(int i, int dir, bitboard edge)
{
	bitboard bit = bb(i);
	bitboard ray = empty;

	while (is_empty(edge & bit)) {
		i += dir;
		bit = bb(i);
		ray |= bit;
	}

	return ray;
}

static void gen_ray_64(bitboard table[64], int dir, bitboard edge)
{
	for (int i = 0; i < 64; ++i)
		table[i] |= gen_ray(i, dir, edge);
}

static void gen_masks(bitboard table[64], int dir_count,
		      const int *dirs, const bitboard *edges)
{
	for (int i = 0; i < dir_count; ++i)
		gen_ray_64(table, dirs[i], edges[i]);
}

static void gen_pawn_attacks(void)
{
	for (int i = 0; i < 64; ++i)
		l_pawn_attacks_north[i] = pawn_reach_north(bb(i));

	for (int i = 0; i < 64; ++i)
		l_pawn_attacks_south[i] = pawn_reach_south(bb(i));
}

static void print_uint64(bitboard value)
{
	printf("C(0x%016" PRIx64 ")", value.as_int());
}

static void print_table(const bitboard table[64], const char *name)
{
	printf("const std::array<bitboard, 64> %s = {\n", name);
	for (int i = 0; i < 64; i+=2) {
		putchar('\t');
		print_uint64(table[i]);
		printf(", ");
		print_uint64(table[i + 1]);
		if (i < 62)
			putchar(',');
		putchar('\n');
	}
	puts("};");
}

static void gen_tables(void)
{
	gen_simple_table(l_king_pattern, king_dirs_v, king_dirs_h);
	gen_simple_table(l_knight_pattern, knight_dirs_v, knight_dirs_h);
	gen_masks(l_diag_masks, 2, bishop_dirs, bishop_edges);
	gen_masks(l_adiag_masks, 2, bishop_dirs + 2, bishop_edges + 2);
	gen_masks(l_hor_masks, 2, rook_dirs, rook_edges);
	gen_masks(l_ver_masks, 2, rook_dirs + 2, rook_edges + 2);
	gen_masks(l_bishop_masks, 4, bishop_dirs, bishop_edges);
	gen_masks(l_rook_masks, 4, rook_dirs, rook_edges);
	gen_pawn_attacks();
}

static void print_tables(void)
{
	print_table(l_king_pattern, "king_pattern");
	putchar('\n');
	print_table(l_knight_pattern, "knight_pattern");
	putchar('\n');
	print_table(l_diag_masks, "diag_masks");
	putchar('\n');
	print_table(l_adiag_masks, "adiag_masks");
	putchar('\n');
	print_table(l_hor_masks, "hor_masks");
	putchar('\n');
	print_table(l_ver_masks, "ver_masks");
	putchar('\n');
	print_table(l_bishop_masks, "bishop_masks");
	putchar('\n');
	print_table(l_rook_masks, "rook_masks");
	putchar('\n');
	print_table(l_pawn_attacks_north, "pawn_attacks_north");
	putchar('\n');
	print_table(l_pawn_attacks_south, "pawn_attacks_south");
}

static void print_prologue()
{
	puts("/* Lookup tables used with bitboards in Taltos */");
	puts("/* Generated file, do not edit manually */");
	puts("");
	puts("#include \"bitboard.h\"");
	puts("");
	puts("namespace taltos");
	puts("{");
	puts("");
	puts("#define C(x) bitboard::from_int(UINT64_C(x))");
	puts("");
}

static void print_epilogue()
{
	puts("}");
}

int main(int argc, char **argv)
{
if (argc > 2)
	return EXIT_FAILURE;

	if (argc > 1) {
		if (freopen(argv[1], "w", stdout) == NULL) {
			perror(argv[1]);
			return EXIT_FAILURE;
		}
	}

	gen_tables();

	print_prologue();
	print_tables();
	print_epilogue();

	return EXIT_SUCCESS;
}
