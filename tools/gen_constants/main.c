
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "macros.h"
#include "bitboard.h"
#include "constants.h"
#include "chess.h"

#include "print.h"
#include "simple_tables.h"
#include "rays.h"
#include "magic.h"
#include "dirs_edges.h"

static void
print_bishop_patterns(void)
{
	uint64_t patterns[64];

	for (int i = 0; i < 64; ++i)
		patterns[i] = gen_move_pattern(i, EMPTY, bishop_dirs, bishop_edges_a);

	print_table(64, patterns, "bishop_pattern_table");
}

static void
print_rook_patterns(void)
{
	uint64_t patterns[64];

	for (int i = 0; i < 64; ++i)
		patterns[i] = (FILE_H << (i & 7)) | (RANK_8 << (i & 0x38));

	print_table(64, patterns, "rook_pattern_table");
}

static void
print_ray_betweens(void)
{
	uint64_t rays[64 * 64];

	gen_ray_between_constants(rays);
	print_table_2d(64, 64, rays, "ray_table");
}

static void
print_king_table(void)
{
	uint64_t table[64];

	gen_king_table(table);
	print_table(64, table, "king_moves_table");
}

static void
print_knight_table(void)
{
	uint64_t table[64];

	gen_knight_table(table);
	print_table(64, table, "knight_moves_table");
}

static void
print_rook_magics(void)
{
	uint64_t magics[MAGICS_ARRAY_SIZE];
	uint64_t attack_results[64 * 0x1000];
	size_t size;

	size = gen_rook_magics(magics, sizeof(attack_results), attack_results);

#   ifdef SLIDING_BYTE_LOOKUP
	uint8_t attack_index8[64*0x1000];

	size_t attack_8_size = transform_sliding_magics(attack_index8);
	print_table_byte(attack_8_size, attack_index8, "rook_attack_index8");
#   endif
	print_table(64 * MAGIC_BLOCK_SIZE, magics, "rook_magics_raw");
	print_table(size, attack_results, "rook_magic_attacks");
}

static void
print_bishop_magics(void)
{
	uint64_t magics[MAGICS_ARRAY_SIZE];
	uint64_t attack_results[64 * 0x1000];
	size_t size;

	size = gen_bishop_magics(magics,
	    			sizeof(attack_results), attack_results);

#   ifdef SLIDING_BYTE_LOOKUP
	uint8_t attack_index8[64*0x1000];

	size_t attack_8_size = transform_sliding_magics(attack_index8);
	print_table_byte(attack_8_size, attack_index8, "bishop_attack_index8");
#   endif
	print_table(64 * MAGIC_BLOCK_SIZE, magics, "bishop_magics_raw");
	print_table(size, attack_results, "bishop_magic_attacks");
}

int
main(void)
{
	puts("");
	puts("#include \"constants.h\"");
	puts("");

	print_king_table();
	puts("");
	print_knight_table();
	puts("");
	print_rook_magics();
	puts("");
	print_bishop_magics();
	puts("");
	print_bishop_patterns();
	puts("");
	print_rook_patterns();
	puts("");
	print_ray_betweens();

	return EXIT_SUCCESS;
}

const int bishop_dirs[4] = {
    EAST + NORTH,
    WEST + NORTH,
    EAST + SOUTH,
    WEST + SOUTH};

const uint64_t bishop_edges_a[4] = {
    FILE_A|RANK_1,
    FILE_H|RANK_1,
    FILE_A|RANK_8,
    FILE_H|RANK_8};

const uint64_t bishop_edges_m[4] = {
    FILE_H|RANK_8,
    FILE_A|RANK_8,
    FILE_H|RANK_1,
    FILE_A|RANK_1};

const int rook_dirs[4] = {EAST, WEST, NORTH, SOUTH};

const uint64_t rook_edges_a[4] = {
    FILE_A,
    FILE_H,
    RANK_1,
    RANK_8};
const uint64_t rook_edges_m[4] = {
    FILE_H,
    FILE_A,
    RANK_8,
    RANK_1};
