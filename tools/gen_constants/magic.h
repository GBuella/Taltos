
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_UTIL_MAGIC_H
#define TALTOS_UTIL_MAGIC_H

#include <stdint.h>
#include <stddef.h>

#ifdef SLIDING_BYTE_LOOKUP
#define MAGIC_BLOCK_SIZE 4
size_t transform_sliding_magics(uint8_t attack_index8[static 64 * 0x1000]);
#else
#define MAGIC_BLOCK_SIZE 3
#endif

#define MAGICS_ARRAY_SIZE (64 * MAGIC_BLOCK_SIZE)

size_t gen_bishop_magics(uint64_t magics[MAGICS_ARRAY_SIZE],
			size_t size, uint64_t attack_results[static size]);

size_t gen_rook_magics(uint64_t magics[MAGICS_ARRAY_SIZE],
			size_t size, uint64_t attack_results[static size]);

#endif
