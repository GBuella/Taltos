/* vim: set filetype=cpp : */
/* vim: set noet tw=100 ts=8 sw=8 cinoptions=+4,(0,t0: */
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

#ifndef TALTOS_FLIP_BB_PAIRS_H
#define TALTOS_FLIP_BB_PAIRS_H

#include "bitboard.h"

namespace taltos
{

/*
 * Flipping piece map bitboard pairs upside down
 * i.e. reversing the order of octets in each 16 octets
 *
 * Example:
 *
 *                    src bitboard pair                  dst bitboard pair
 *
 * player's bitboard:     .1....1.                            ...1....
 *                        ..1..1..                            ...1....
 *                        ...11...                            ...1....
 *                        ...11... ____          ___________> ...1....
 *                        ..1..1..     \        /             ...1....
 *                        .1....1.      \      /              ..1.1...
 *                        1......1       \    /               .1...1..
 *                        ........        \  /                1.....1.
 *                                         \/
 * opponent's bitboard:   1.....1.         /\                 ........
 *                        .1...1..        /  \                1......1
 *                        ..1.1...       /    \               .1....1.
 *                        ...1....      /      \____________> ..1..1..
 *                        ...1.... ____/                      ...11...
 *                        ...1....                            ...11...
 *                        ...1....                            ..1..1..
 *                        ...1....                            .1....1.
 *
 */

// TODO flip_4_bb_pairs using AVX512

#elif __has_include(<x86intrin.h>) && __AVX2
/*
 * __m256i _mm256_shuffle_epi8(__m256i a, __m256i b)
 * Instruction: vpshufb ymm, ymm, ymm
 * CPUID Flags: AVX2
 *
 * Shuffle 8-bit integers in a within 128-bit lanes according to shuffle control
 * mask in the corresponding 8-bit element of b, and store the results in dst.
 *
 */

static_assert(pos_alignment % 32 == 0, "alignment error");

static inline void
flip_2_bb_pairs(uint64_t *restrict dst, const uint64_t *restrict src)
{
/* BEGIN CSTYLED */
	const __m256i key = _mm256_setr_epi8(15, 14, 13, 12, 11, 10,  9,  8,
					      7,  6,  5,  4,  3,  2,  1,  0,
					     15, 14, 13, 12, 11, 10,  9,  8,
					      7,  6,  5,  4,  3,  2,  1,  0);
/* END CSTYLED */

	__m256i vector;
	__m256i *dst256 = (__m256i*)dst;
	const __m256i *src256 = (const __m256i*)src;

	vector = _mm256_load_si256(src256);
	vector = _mm256_shuffle_epi8(vector, key);
	_mm256_store_si256(dst256, vector);
}

#elif __has_include(<x86intrin.h>) && __SSE2 \
	&& !defined(TALTOS_FORCE_NO_SSE) && 0
// TODO

/*
 * __m128i _mm_shuffle_epi8(__m128i a, __m128i b)
 * Instruction: pshufb mm, mm
 * CPUID Flags: SSSE3
 *
 * Shuffle packed 8-bit integers in a according to shuffle control mask
 * in the corresponding 8-bit element of b, and store the results in dst.
 *
 *
 */

static inline void
flip_2_bb_pairs(uint64_t *restrict dst, const uint64_t *restrict src)
{
/* BEGIN CSTYLED */
	const __m128i key = _mm_setr_epi8(15, 14, 13, 12, 11, 10,  9,  8,
					   7,  6,  5,  4,  3,  2,  1,  0);
/* END CSTYLED */

	__m128i vector;
	__m128i *dst128 = (__m128i*)dst;
	const __m128i *src128 = (const __m128i*)src;

	vector = _mm_load_si128(src128);
	vector = _mm_shuffle_epi8(vector, key);
	_mm_store_si128(dst128, vector);
	vector = _mm_load_si128(src128 + 1);
	vector = _mm_shuffle_epi8(vector, key);
	_mm_store_si128(dst128 + 1, vector);
}

#else

static inline void
flip_2_bb_pairs(uint64_t *restrict dst attribute(align_value(32)),
	const uint64_t *restrict src attribute(align_value(32)))
{
	dst[0] = bswap(src[1]);
	dst[1] = bswap(src[0]);
	dst[2] = bswap(src[3]);
	dst[3] = bswap(src[2]);
}

#endif

}

#endif
