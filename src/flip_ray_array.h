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

#ifndef TALTOS_FLIP_RAY_ARRAY_H
#define TALTOS_FLIP_RAY_ARRAY_H

#include "macros.h"
#include "bitboard.h"

#ifdef TALTOS_CAN_USE_INTEL_AVX2

static void
flip_ray_array(uint64_t dst[restrict 64], const uint64_t src[restrict 64])
{
	const __m256i key = _mm256_setr_epi8( 7,  6,  5,  4,  3,  2,  1,  0,
	                                     15, 14, 13, 12, 11, 10,  9,  8,
	                                      7,  6,  5,  4,  3,  2,  1,  0,
	                                     15, 14, 13, 12, 11, 10,  9,  8);


	int rs = 0;
	int rd = 48;
	do {
		const __m256i *restrict src32 = (const __m256i*)(src + rs);
		__m256i *restrict dst32 = (__m256i*)(dst + rd);

		__m256i vector0 = _mm256_load_si256(src32 + 2);
		__m256i vector1 = _mm256_load_si256(src32 + 3);
		__m256i vector2 = _mm256_load_si256(src32 + 0);
		__m256i vector3 = _mm256_load_si256(src32 + 1);
		dst32[0] = _mm256_shuffle_epi8(vector0, key);
		dst32[1] = _mm256_shuffle_epi8(vector1, key);
		dst32[2] = _mm256_shuffle_epi8(vector2, key);
		dst32[3] = _mm256_shuffle_epi8(vector3, key);

		rs += 16;
		rd -= 16;
	} while (rs < 64);
}

#elif defined(TALTOS_CAN_USE_INTEL_AVX) && 0
// XXX it actually is SSSE3

static void
flip_ray_array(uint64_t dst[restrict 64], const uint64_t src[restrict 64])
{
	int rs = 0;
	int rd = 56;
	do {
		const __m128i *restrict src16 = (const __m128i*)(src + rs);
		__m128i *restrict dst16 = (__m128i*)(dst + rd);

		dst16[0] = _mm_shuffle_epi8(src16[0], ray_flip_shufflekey_16());
		dst16[1] = _mm_shuffle_epi8(src16[1], ray_flip_shufflekey_16());
		dst16[2] = _mm_shuffle_epi8(src16[2], ray_flip_shufflekey_16());
		dst16[3] = _mm_shuffle_epi8(src16[3], ray_flip_shufflekey_16());

		rs += 8;
		rd -= 8;
	} while (rs < 64);
}

#else

static inline void
flip_ray_array(uint64_t dst[restrict 64] attribute(align_value(pos_alignment)),
		const uint64_t src[restrict 64] attribute(align_value(pos_alignment)))
{
	int rs = 0;
	int rd = 56;
	do {
		dst[rd + 0] = bswap(src[rs + 0]);
		dst[rd + 1] = bswap(src[rs + 1]);
		dst[rd + 2] = bswap(src[rs + 2]);
		dst[rd + 3] = bswap(src[rs + 3]);
		dst[rd + 4] = bswap(src[rs + 4]);
		dst[rd + 5] = bswap(src[rs + 5]);
		dst[rd + 6] = bswap(src[rs + 6]);
		dst[rd + 7] = bswap(src[rs + 7]);
		rs += 8;
		rd -= 8;
	} while (rs < 64);
}

#endif

#endif
