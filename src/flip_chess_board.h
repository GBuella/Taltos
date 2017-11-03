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

#ifndef TALTOS_FLIP_CHESS_BOARD_H
#define TALTOS_FLIP_CHESS_BOARD_H

#include "macros.h"
#include <string.h>

#if defined(TALTOS_CAN_USE_INTEL_AVX2)
/*
 * __m256i _mm256_permute4x64_epi64(__m256i a, const int imm8)
 * Instruction: vpermq ymm, ymm, imm
 * CPUID Flags: AVX2
 *
 * Shuffle 64-bit integers in a across lanes using the control in imm8, and
 * store the results in dst.
 *
 *
 *     src               _mm256_permute4x64_epi64          dst
 *
 *  +------+                                             +------+
 *  |RANK_8|                                             |RANK_1|
 *  |RANK_7|         +------+          +------+          |RANK_2|
 *  |RANK_6|         |RANK_4|          |RANK_1| ------>  |RANK_3|
 *  |RANK_5|         |RANK_3|          |RANK_2|          |RANK_4|
 *  |RANK_4|         |RANK_2| -------> |RANK_3|          |      |
 *  |RANK_3| ------> |RANK_1|          |RANK_4|          |      |
 *  |RANK_2|         +------+          +------+          |      |
 *  |RANK_1|                                             |      |
 *  +------+                                             +------+
 *
 *  +------+                                             +------+
 *  |RANK_8|                                             |RANK_1|
 *  |RANK_7|         +------+          +------+          |RANK_2|
 *  |RANK_6| ------> |RANK_8|          |RANK_5|          |RANK_3|
 *  |RANK_5|         |RANK_7|          |RANK_6|          |RANK_4|
 *  |RANK_4|         |RANK_6| -------> |RANK_7|          |RANK_5|
 *  |RANK_3|         |RANK_5|          |RANK_8| ------>  |RANK_6|
 *  |RANK_2|         +------+          +------+          |RANK_7|
 *  |RANK_1|                                             |RANK_8|
 *  +------+                                             +------+
 */

static inline void
flip_chess_board(unsigned char dst[], const unsigned char src[])
{
	__m256i vector;
	__m256i *restrict dst256 = (__m256i*)dst;
	const __m256i *restrict src256 = (const __m256i*)src;

	vector = _mm256_load_si256(src256);
	vector = _mm256_permute4x64_epi64(vector, _MM_SHUFFLE(0, 1, 2, 3));
	_mm256_store_si256(dst256 + 1, vector);
	vector = _mm256_load_si256(src256 + 1);
	vector = _mm256_permute4x64_epi64(vector, _MM_SHUFFLE(0, 1, 2, 3));
	_mm256_store_si256(dst256, vector);
}

#elif defined(TALTOS_CAN_USE_INTEL_AVX)
/*
 * __m256d _mm256_permute_pd(__m256d a, int imm8)
 * Instruction: vpermilpd ymm, ymm, imm
 * CPUID Flags: AVX
 *
 * Shuffle double-precision (64-bit) floating-point elements in a within 128-bit
 * lanes using the control in imm8, and store the results in dst.
 *
 *
 * __m256d _mm256_permute2f128_pd (__m256d a, __m256d b, int imm8)
 * Instruction: vperm2f128 ymm, ymm, ymm, imm
 * CPUID Flags: AVX
 *
 * Shuffle 128-bits (composed of 2 packed double-precision (64-bit)
 * floating-point elements) selected by imm8 from a and b, and store the
 * results in dst.
 *
 *
 *                     _mm256_load_pd
 *                       :
 *                       :           _mm256_permute_pd
 *                       :             :
 *                       :             :           _mm256_permute2f128_pd
 *                       :             :             :
 *                       :             :             :           _mm256_store_pd
 *                       :             :             :             :
 *                       :             :             :             :
 *              src      :             :             :             :    dst
 *             +------+  :             :             :             :   +------+
 * First copy  |RANK_8|  :             :             :             :   |RANK_1|
 * the lower   |RANK_7|  :   +------+  :   +------+  :   +------+  :   |RANK_2|
 * four ranks  |RANK_6|  :   |RANK_4|  :   |RANK_3|  :   |RANK_1| ---> |RANK_3|
 * from src to |RANK_5|  :   |RANK_3|  :   |RANK_4|  :   |RANK_2|      |RANK_4|
 * the upper   |RANK_4|  :   |RANK_2| ---> |RANK_1| ---> |RANK_3|      |      |
 * four ranks  |RANK_3| ---> |RANK_1|      |RANK_2|      |RANK_4|      |      |
 * in dst.     |RANK_2|      +------+      +------+      +------+      |      |
 *             |RANK_1|                                                |      |
 *             +------+                                                +------+
 *
 *             +------+                                                +------+
 * Then copy   |RANK_8|                                                |RANK_1|
 * the upper   |RANK_7|      +------+      +------+      +------+      |RANK_2|
 * four ranks  |RANK_6|      |RANK_4|      |RANK_3|      |RANK_1| ---> |RANK_3|
 * from src to |RANK_5|      |RANK_3|      |RANK_4|      |RANK_2|      |RANK_4|
 * the lower   |RANK_4|      |RANK_2| ---> |RANK_1| ---> |RANK_3|      |      |
 * four ranks  |RANK_3| ---> |RANK_1|      |RANK_2|      |RANK_4|      |      |
 * in dst.     |RANK_2|      +------+      +------+      +------+      |      |
 *             |RANK_1|                                                |      |
 *             +------+                                                +------+
 */

static inline void
flip_chess_board(unsigned char dst[], const unsigned char src[])
{
	__m256d vector;
	double *restrict dst256_0 = (double*)dst;
	double *restrict dst256_1 = (double*)(dst + 32);
	const double *restrict src256_0 = (const double*)src;
	const double *restrict src256_1 = (const double*)(src + 32);

	vector = _mm256_load_pd(src256_0);
	vector = _mm256_permute_pd(vector, 1 + (1 << 2));
	vector = _mm256_permute2f128_pd(vector, vector, 1);
	_mm256_store_pd(dst256_1, vector);
	vector = _mm256_load_pd(src256_1);
	vector = _mm256_permute_pd(vector, 1 + (1 << 2));
	vector = _mm256_permute2f128_pd(vector, vector, 1);
	_mm256_store_pd(dst256_0, vector);
}

#else
/*
 * No intrinsics, copy rank by rank.
 */

static inline void
flip_chess_board(unsigned char dst[], const unsigned char src[])
{
	for (int i = 0; i < 64; i += 8)
		memcpy(dst + (56 - i), src + i, 8);
}

#endif

#endif
