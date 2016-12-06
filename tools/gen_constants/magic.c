
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#include "magic.h"
#include "rays.h"
#include "bitboard.h"
#include "dirs_edges.h"

#include <stdlib.h>

static size_t
gen_magics(uint64_t magics[static restrict MAGICS_ARRAY_SIZE],
		size_t size, uint64_t attack_results[static restrict size],
		const uint64_t masks[static restrict 64],
		const int dirs[static restrict 4],
		const uint64_t edges[static restrict 4]);

size_t
gen_bishop_magics(uint64_t magics[MAGICS_ARRAY_SIZE],
		size_t size, uint64_t attack_results[static size])
{
	uint64_t masks[64];

	gen_pre_masks(masks, bishop_dirs, bishop_edges_m);
	return gen_magics(magics, size, attack_results, masks,
				bishop_dirs, bishop_edges_a);
}

size_t
gen_rook_magics(uint64_t magics[MAGICS_ARRAY_SIZE],
		size_t size, uint64_t attack_results[static size])
{

	uint64_t masks[64];

	gen_pre_masks(masks, rook_dirs, rook_edges_m);
	return gen_magics(magics, size, attack_results, masks,
				rook_dirs, rook_edges_a);
}



/*
 * fill_attack_boards - Generate each possible occupancy map
 * allowed by the mask. For each occupancy map, generate the
 * attacks of a piece considering that specific occupancy.
 */
static void
fill_attack_boards(int sq_i,
	       uint64_t occs[static 0x1001],
	       uint64_t attacks[static 0x1001],
	       const int dirs[static 4],
	       const uint64_t edges[static 4],
	       uint64_t mask)
{
	*occs++ = bit64(sq_i);
	*attacks++ = gen_move_pattern(sq_i, bit64(sq_i), dirs, edges);
	for (uint64_t occ = mask; is_nonempty(occ); occ = (occ-1) & mask) {
		*occs = occ | bit64(sq_i);
		*attacks = gen_move_pattern(sq_i, *occs, dirs, edges);
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

/*
 * search_magic - generate random constants, until one of them
 * is deemed suitable for use as a magic multiplier for the magic
 * bitboards atttacks of a piece from a specific src square.
 * Store the constants needed for the magic bitboards lookup
 * in the pointed to by *pmagic. These can be used to lookup the
 * attack bitboards stored at *attack_results.
 */
static void
search_magic(uint64_t *pmagic,
    		uint64_t *attack_results,
		const uint64_t *occs,
		const uint64_t *attacks,
		uint64_t mask,
		uint64_t src,
		size_t *size)
{
	// Where to store the magic constants for this piece/src square
	uint64_t *results = attack_result + *size;

	// How many bits are needed to encode an occupancy map?
	unsigned width = popcnt(mask);

	// How many different occupancy maps are possible?
	unsigned count = 1 << width;

	memset(results, 0, count * sizeof(*results));

	for (int tries = 0; tries < 1000000000; ++tries) {
		// Make a guess for the magic multiplier
		uint64_t magic = random_magic();

		if (popcnt((src|mask) * magic) < 9)
			continue;

		/*
		 * Loop over each occupancy map, and check wether the magic
		 * multiplier works with it. It must work with every occupancy
		 * map.
		 */
		const uint64_t *occ = occs;
		const uint64_t *attack = attacks;
		unsigned max = 0;

		while (is_nonempty(*occ)) {
			/*
			 * Perform the multiplication that would be done during
			 * move generation. Shift right by (64 - width), this leaves
			 * width valueable bits, and the left (64 - width) bits are
			 * zeroed. This is used to look up the attacks in an array
			 * of size count.
			 */
			unsigned index = (((*occ & mask) * magic) >> (64 - width));

			if (is_empty(results[index])) {
				/*
				 * This entry is not yet used, store the
				 * appropriate attack bitboard here. Note:
				 * there is empty attack possible, even if
				 * the piece is surronded by other pieces
				 * according to the occupancy map, it can reach
				 * the neighbouring squares, which is reflected
				 * in the attack bitboard.
				 */
				results[index] = *attack;
#ifdef SLIDING_BYTE_LOOKUP
				results[index] |= src;
#endif

				/*
				 * Remember which was the largest index used.
				 */
				if (max < index)
					max = index;
			}
			else if (results[index] != (src|*attack)) {
				/*
				 * The entry is already used, and it stores a
				 * different attack bitboard.
				 * Therefore, the magic multiplier guess is
				 * wrong.
				 */
				break;
			}
			++occ;
			++attack;
		}

		if (is_empty(*occ)) { // Was the previous checking loop successfull?
			pmagic[0] = mask;
			pmagic[1] = magic;
			pmagic[2] = (64-width) | (*size << 8);
			*size += max+1;
			return;
		}
		memset(results, 0, (max+1) * sizeof *results);
	}
	fprintf(stderr, "Not found\n");
	exit(EXIT_FAILURE);
}

#ifdef SLIDING_BYTE_LOOKUP

size_t
transform_sliding_magics(uint8_t attack_index8[static 64 * 0x1000])
{
	unsigned attack_offset_new = 0;
	size_t size = 0;

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
	return size;
}

#endif



static size
gen_magics(uint64_t magics[static restrict MAGICS_ARRAY_SIZE],
		size_t size, uint64_t attack_results[static restrict size],
		const uint64_t masks[static restrict 64],
		const int dirs[static restrict 4],
		const uint64_t edges[static restrict 4])
{
	uint64_t occs[0x1001];
	uint64_t attacks[0x1001];
	size_t size = 0;

	memset(attack_results, 0, size * sizeof(attack_results[0]));
	memset(magics, 0, MAGICS_ARRAY_SIZE * sizeof(magics[0]));

	for (int i = 0; i < 64; ++i) {
		fill_attack_boards(i, occs, attacks, dirs, edges, masks[i]);
		search_magic(magics + i * MAGIC_BLOCK_SIZE, attack_results,
		    occs, attacks,
		    masks[i], bit64(i), &size);
	}

	return size;
}
