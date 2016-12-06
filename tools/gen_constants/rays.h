
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=(4: */

#ifndef TALTOS_UTIL_RAYS_H
#define TALTOS_UTIL_RAYS_H

#include "macros.h"

#include <stdint.h>

void gen_ray_between_constants(uint64_t table[static 64 * 64]);

uint64_t gen_move_pattern(int src_i,
                 uint64_t occ,
                 const int dirs[static restrict 4],
                 const uint64_t edges[static restrict 4]);

void gen_pre_masks(uint64_t masks[static restrict 64],
		const int dirs[static restrict 4],
		const uint64_t edges[static restrict 4]);

#endif
