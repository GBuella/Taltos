
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "macros.h"
#include "bitboard.h"
#include "constants.h"
#include "chess.h"

static const int king_dirs_h[] = { 1, 1, 1, 0, -1, -1, -1, 0};
static const int king_dirs_v[] = { -1, 0, 1, 1, 1, 0, -1, -1};

static const int knight_dirs_h[] = { -2, -1, -2, -1,  2,  1,  2,  1};
static const int knight_dirs_v[] = { -1, -2,  1,  2, -1, -2,  1,  2};

static const int rook_dirs[4] = {EAST, WEST, NORTH, SOUTH};

static const uint64_t rook_edges_a[4] = {
    FILE_A,
    FILE_H,
    RANK_1,
    RANK_8};
static const uint64_t rook_edges_m[4] = {
    FILE_H,
    FILE_A,
    RANK_8,
    RANK_1};

static const int bishop_dirs[4] = {
    EAST + NORTH,
    WEST + NORTH,
    EAST + SOUTH,
    WEST + SOUTH};

static const uint64_t bishop_edges_a[4] = {
    FILE_A|RANK_1,
    FILE_H|RANK_1,
    FILE_A|RANK_8,
    FILE_H|RANK_8};

static const uint64_t bishop_edges_m[4] = {
    FILE_H|RANK_8,
    FILE_A|RANK_8,
    FILE_H|RANK_1,
    FILE_A|RANK_1};

static uint64_t masks[64];
#ifdef SLIDING_BYTE_LOOKUP
#define MAGIC_BLOCK_SIZE 4
static uint64_t magics[64*4];
static uint8_t attack_index8[64*0x1000];
static size_t attack_8_size;
#else
#define MAGIC_BLOCK_SIZE 3
static uint64_t magics[64*3];
#endif
static uint64_t attack_results[64*0x1000];
static unsigned attack_result_i;

static void gen_simple_table(const int dirs_v[8],
                             const int dirs_h[8])
{
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            for (int dir_i = 0; dir_i < 8; ++dir_i) {
                int r = rank + dirs_v[dir_i];
                int f = file + dirs_h[dir_i];
                if (r >= 0 && r <= 7 && f >= 0 && f <= 7) {
                    masks[ind(rank, file)] |= bit64(ind(r, f));
                }
            }
        }
    }
}

static void
print_table(size_t size, const uint64_t table[size], const char *name)
{
    printf("const uint64_t %s[%zu] = {\n", name, size);
    printf("0x%016" PRIX64, table[0]);
    for (size_t i = 1; i < size; ++i) {
        printf(",%s0x%016" PRIX64, (i % 4 == 0) ? "\n" : "", table[i]);
    }
    puts("\n};\n");
}

static void print_table_2d(size_t s0, size_t s1,
                           const uint64_t table[s0*s1], 
                           const char *name)
{
    printf("const uint64_t %s[%zu][%zu] = {\n", name, s0, s1);
    for (size_t i = 0; i < s0; ++i) {
        printf("{\n0x%016" PRIX64, table[s0 * i + 0]);
        for (size_t j = 1; j < s1; ++j) {
            uint64_t value = table[i * s0 + j];
            printf(",%s0x%016" PRIX64, (j % 4 == 0) ? "\n " : "", value);
        }
        printf("\n}%s\n", (i + 1 < s0) ? "," : "");
    }
    puts("\n};\n");
}

#ifdef SLIDING_BYTE_LOOKUP

static void
print_table_byte(size_t size, const uint8_t table[size], const char *name)
{
    printf("const uint8_t %s[%zu] = {\n", name, size);
    printf("0x%02" PRIX8, table[0]);
    for (size_t i = 1; i < size; ++i) {
        printf(",%s0x%02" PRIX8, (i % 8 == 0) ? "\n" : "", table[i]);
    }
    puts("\n};\n");
}

#endif

static void gen_pre_mask_ray(int dir, uint64_t edge)
{
    for (int i = 0; i < 64; ++i) {
        if (is_nonempty(bit64(i) & edge)) continue;

        int ti = i+dir;
        uint64_t bit = bit64(ti);
        while (is_empty(bit & edge)) {
            masks[i] |= bit;
            ti += dir;
            bit = bit64(ti);
        }
    }
}

static void
gen_pre_masks(const int dirs[4], const uint64_t edges[4])
{
    for (int i = 0; i < 4; ++i) {
        gen_pre_mask_ray(dirs[i], edges[i]);
    }
}

static uint64_t gen_ray(int src_i, uint64_t occ, int dir, uint64_t edge)
{
    uint64_t result = EMPTY;
    int i = src_i + dir;
    if (!ivalid(i)) return EMPTY;
    uint64_t bit = bit64(i);
    while (is_empty(bit & edge)) {
        result |= bit;
        if (is_nonempty(occ & bit)) return result;
        i += dir;
        if (!ivalid(i)) return result;
        bit = bit64(i);
    }
    return result;
}

static uint64_t
gen_move_pattern(int src_i,
                 uint64_t occ,
                 const int dirs[4],
                 const uint64_t edges[4])
{
    return gen_ray(src_i, occ, dirs[0], edges[0])
         | gen_ray(src_i, occ, dirs[1], edges[1])
         | gen_ray(src_i, occ, dirs[2], edges[2])
         | gen_ray(src_i, occ, dirs[3], edges[3]);
}

static void add_rays(int r, int f, int r_dir, int f_dir)
{
    int src_i = ind(r, f);
    uint64_t ray = EMPTY;
    int dst_r = r + r_dir;
    int dst_f = f + f_dir;

    while (is_valid_rank(dst_r) && is_valid_file(dst_f)) {
        int dst_i = ind(dst_r, dst_f);

        attack_results[src_i*64 + dst_i] = ray;
        attack_results[dst_i*64 + src_i] = ray;
        ray |= bit64(dst_i);
        dst_r += r_dir;
        dst_f += f_dir;
    }
}

static void gen_ray_constants(void)
{
    memset(attack_results, 0, 64*64 * sizeof *attack_results);

    for (int r = rank_8; is_valid_rank(r); r += RSOUTH) {
        for (int f = file_a; is_valid_file(f); f += EAST) {
            add_rays(r, f, RSOUTH, 0);
            add_rays(r, f, 0, WEST);
            add_rays(r, f, RSOUTH, EAST);
            add_rays(r, f, RSOUTH, WEST);
        }
    }
}

static void fill_attack_boards(int sq_i,
                               uint64_t *occs,
                               uint64_t *attacks,
                               const int dirs[4],
                               const uint64_t edges[4],
                               uint64_t mask)
{
    *occs++ = bit64(sq_i);
    *attacks++ = gen_move_pattern(sq_i, bit64(sq_i), dirs, edges);
    for (uint64_t occ = mask; is_nonempty(occ); occ = (occ-1) & mask) {
        *occs = occ | bit64(sq_i);
        *attacks = gen_move_pattern(sq_i, occ|bit64(sq_i), dirs, edges);
        occs++;
        attacks++;
    }
    *occs = EMPTY;
    *attacks = EMPTY;
}

static uint64_t
random_magic(void)
{
	if (RAND_MAX >= INT32_MAX)
		return (rand() & rand() & rand())
		    + (((uint64_t)(rand() & rand() &  rand())) << 32);
	else if (RAND_MAX >= INT16_MAX)
		return (rand() & rand() & rand())
		    + (((uint64_t)(rand() & rand() &  rand())) << 16)
		    + (((uint64_t)(rand() & rand() &  rand())) << 32)
		    + (((uint64_t)(rand() & rand() &  rand())) << 48);
	else
		exit(EXIT_FAILURE);
}

static void search_magic(uint64_t *pmagic,
                         const uint64_t *occs,
                         const uint64_t *attacks,
                         uint64_t mask,
                         uint64_t src)
{
    uint64_t *results = attack_results + attack_result_i;
    unsigned width = popcnt(mask);

    memset(results, 0, (1<<width) * sizeof *results);
    for (int tries = 0; tries < 1000000000; ++tries) {
        uint64_t magic = random_magic();

        if (popcnt((src|mask) * magic) < 9) {
            continue;
        }
        const uint64_t *occ = occs;
        const uint64_t *attack = attacks;
        unsigned max = 0;

        while (is_nonempty(*occ)) {
            unsigned index = (((*occ & mask) * magic) >> (64-width));

            if (is_empty(results[index])) {
                results[index] = *attack|src;
                if (max < index) {
                    max = index;
                }
            }
            else if (results[index] != (src|*attack)) {
                break;
            }
            ++occ;
            ++attack;
        }
        if (is_empty(*occ)) {
            pmagic[0] = mask;
            pmagic[1] = magic;
            pmagic[2] = (64-width) | (attack_result_i << 8);
#           ifndef SLIDING_BYTE_LOOKUP
            for (unsigned i = 0; i <= max; ++i) {
                results[i] &= ~src;
            }
#           endif
            attack_result_i += max+1;
            return;
        }
        memset(results, 0, (max+1) * sizeof *results);
    }
    fprintf(stderr, "Not found\n");
    exit(EXIT_FAILURE);
}

static void gen_magics(const int dirs[4],
                       const uint64_t edges[4])
{
    uint64_t occs[0x1001];
    uint64_t attacks[0x1001];

    attack_result_i = 0;
    for (int i = 0; i < 64; ++i) {
        fill_attack_boards(i, occs, attacks, dirs, edges, masks[i]);
        search_magic(magics+i*MAGIC_BLOCK_SIZE, occs, attacks,
                     masks[i], bit64(i));
    }
}

static void gen_bishop_patterns(void)
{
    for (int i = 0; i < 64; ++i) {
        masks[i] = gen_move_pattern(i, EMPTY, bishop_dirs, bishop_edges_a);
    }
}

static void gen_rook_patterns(void)
{
    for (int i = 0; i < 64; ++i) {
        masks[i] = (FILE_H << (i & 7)) | (RANK_8 << (i & 0x38));
    }
}

#ifdef SLIDING_BYTE_LOOKUP

static void transform_sliding_magics(void)
{
    unsigned attack_offset_new = 0;
    attack_8_size = 0;
    for (int i = 0; i < 64; ++i) {
        uint64_t attack_array[0x100];
        unsigned attack_array_l = 0;
        unsigned attack_offset_old = magics[i*4+2]>>8;
        unsigned attack_count;

        memset(attack_array, 0, sizeof attack_array);
        if (i == 63) {
            attack_count = attack_result_i - attack_offset_old;
        }
        else {
            attack_count = (magics[(i+1)*4+2]>>8) - attack_offset_old;
        }
        for (unsigned j = 0; j < attack_count; ++j) {
            uint64_t attack = attack_results[attack_offset_old+j];
            if (is_empty(attack)) continue;
            unsigned k;
            for (k = 0; k < attack_array_l; ++k) {
                if (attack_array[k] == attack) {
                    attack_index8[attack_offset_old+j] = k;
                    break;
                }
            }
            if (k == attack_array_l) {
                attack_array[attack_array_l] = attack;
                attack_index8[attack_offset_old+j] = k;
                ++attack_array_l;
            }
        }
        for (unsigned j = 0; j < attack_array_l; ++j) {
            attack_results[attack_offset_new+j] = attack_array[j] & ~bit64(i);
        }
        magics[i*4+3] = attack_offset_new;
        attack_offset_new += attack_array_l;
    }
    attack_8_size = attack_result_i;
    attack_result_i = attack_offset_new;
}

#endif

int main(void)
{
    printf("\n#include \"constants.h\"\n\n");

    gen_simple_table(king_dirs_v, king_dirs_h);
    print_table(64, masks, "king_moves_table");

    memset(masks, 0, sizeof masks);
    gen_simple_table(knight_dirs_v, knight_dirs_h);
    print_table(64, masks, "knight_moves_table");

    memset(masks, 0, sizeof masks);
    gen_pre_masks(rook_dirs, rook_edges_m);
    gen_magics(rook_dirs, rook_edges_a);
#   ifdef SLIDING_BYTE_LOOKUP
    transform_sliding_magics();
    print_table_byte(attack_8_size, attack_index8, "rook_attack_index8");
#   endif
    print_table(64*MAGIC_BLOCK_SIZE, magics, "rook_magics_raw");
    print_table(attack_result_i, attack_results, "rook_magic_attacks");

    memset(masks, 0, sizeof masks);
    gen_pre_masks(bishop_dirs, bishop_edges_m);
    /* print_table(64, masks, "bishop_masks"); */
    gen_magics(bishop_dirs, bishop_edges_a);
#   ifdef SLIDING_BYTE_LOOKUP
    transform_sliding_magics();
    print_table_byte(attack_8_size, attack_index8, "bishop_attack_index8");
#   endif
    print_table(64*MAGIC_BLOCK_SIZE, magics, "bishop_magics_raw");
    print_table(attack_result_i, attack_results, "bishop_magic_attacks");

    gen_bishop_patterns();
    print_table(64, masks, "bishop_pattern_table");

    gen_rook_patterns();
    print_table(64, masks, "rook_pattern_table");

    gen_ray_constants();
    print_table_2d(64, 64, attack_results, "ray_table");

    return EXIT_SUCCESS;
}

