/* vim: set filetype=c : */
/* vim: set noet tw=80 ts=8 sw=8 cinoptions=+4,(0,t0: */

#include "chess.h"
#include "bitboard.h"
#include "constants.h"
#include "position.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint64_t l_knight_pattern[64];
static uint64_t l_king_pattern[64];
static uint64_t l_diag_masks[64];
static uint64_t l_adiag_masks[64];
static uint64_t l_hor_masks[64];
static uint64_t l_ver_masks[64];
static uint64_t l_bishop_masks[64];
static uint64_t l_rook_masks[64];
static uint64_t l_pawn_attacks_north[64];

static const int king_dirs_h[] = { 1, 1, 1, 0, -1, -1, -1, 0};
static const int king_dirs_v[] = { -1, 0, 1, 1, 1, 0, -1, -1};

static const int knight_dirs_h[] = { -2, -1, -2, -1,  2,  1,  2,  1};
static const int knight_dirs_v[] = { -1, -2,  1,  2, -1, -2,  1,  2};

static const int rook_dirs[4] = {EAST, WEST, NORTH, SOUTH};

static const uint64_t rook_edges[4] = { FILE_H, FILE_A, RANK_8, RANK_1};

static const int bishop_dirs[4] = {
	EAST + SOUTH,
	WEST + NORTH,
	WEST + SOUTH,
	EAST + NORTH};

static const uint64_t bishop_edges[4] = {
	FILE_H|RANK_1,
	FILE_A|RANK_8,
	FILE_A|RANK_1,
	FILE_H|RANK_8};

static void
gen_simple_table(uint64_t table[64], const int dirs_v[8], const int dirs_h[8])
{
	for (int rank = 0; rank < 8; ++rank) {
		for (int file = 0; file < 8; ++file) {
			for (int dir_i = 0; dir_i < 8; ++dir_i) {
				int r = rank + dirs_v[dir_i];
				int f = file + dirs_h[dir_i];
				if (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
					int i = ind(r, f);
					table[ind(rank, file)] |= bit64(i);
				}
			}
		}
	}
}

static uint64_t
gen_ray(int i, int dir, uint64_t edge)
{
	uint64_t bit = bit64(i);
	uint64_t ray = EMPTY;

	while (is_empty(edge & bit)) {
		i += dir;
		bit = bit64(i);
		ray |= bit;
	}

	return ray;
}

static void
gen_ray_64(uint64_t table[64], int dir, uint64_t edge)
{
	for (int i = 0; i < 64; ++i)
		table[i] |= gen_ray(i, dir, edge);
}

static void
gen_masks(uint64_t table[64], int dir_count,
	  const int *dirs, const uint64_t *edges)
{
	for (int i = 0; i < dir_count; ++i)
		gen_ray_64(table, dirs[i], edges[i]);
}

static void
gen_pawn_attacks(void)
{
	for (int i = 0; i < 64; ++i)
		l_pawn_attacks_north[i] = pawn_reach_north(bit64(i));
}

static void
print_uint64(uint64_t value)
{
	printf("UINT64_C(0x%016" PRIx64 ")", value);
}

static void
print_table(const uint64_t table[64], const char *name)
{
	printf("const uint64_t %s[64] = {\n", name);
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

static void
gen_tables(void)
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

static void
print_tables(void)
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
}

static void
print_prologue(const char *guard)
{
	puts("/* Lookup tables used with bitboards in Taltos */");
	puts("/* Generated file, do not edit manually */");
	puts("");
	printf("#ifndef %s\n", guard);
	printf("#define %s\n", guard);
	puts("");
	puts("#include \"constants.h\"");
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
	const char *guard = "TALTOS_BITBOARD_CONSTANTS_INC";

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

	gen_tables();

	print_prologue(guard);
	putchar('\n');
	print_tables();
	putchar('\n');
	print_epilogue();

	return EXIT_SUCCESS;
}
