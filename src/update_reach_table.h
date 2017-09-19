
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#ifndef TALTOS_UPDATE_REACH_TABLE_H
#define TALTOS_UPDATE_REACH_TABLE_H

#include "macros.h"
#include "chess.h"

#ifdef TALTOS_CAN_USE_INTEL_AVX512VBMI

static inline void
update_reach_table(uint8_t *restrict dst,
			const uint8_t *restrict src,
			const uint8_t *restrict opposite_src,
    			move m)
{
	__m512i vector = _mm512_load_si512((const __mm512i*)src);

	uint8_t from = mfrom(m);
	uint8_t to = mto(m);

	uint8_t reach = opposite_src[from];
	__m512i broadcast = _mm512_set1_epi8(src[from]);

	__m512i mask = move_reach_masks[from][reach];

	// AVX512VBMI
	vector = _mm512_permutex2var_epi8(vector, mask, broadcast);

	reach = opposite_src[to];
	broadcast = _mm512_set1_epi8(to);

	__m512i mask = move_reach_masks[to][reach];

	// AVX512VBMI
	vector = _mm512_permutex2var_epi8(vector, mask, broadcast);

	vector = _mm512_permutexvar_epi64(key, vector); // AVX512F
	mask = _mm512_set1_epi8(0x38);
	vector = _mm512_xor_si512(vector, mask); // AVX512F

	_mm512_store_si512((__m512i*)dst, vector);
}

#elif defined(TALTOS_CAN_USE_INTEL_AVX2)

static inline void
update_reach_table(uint8_t *restrict dst,
			const uint8_t *restrict src,
			const uint8_t *restrict opposite_src,
    			move m)
{
	__m256i vector0 = _mm256_load_si256((const __mm256i*)src);
	__m256i vector1 = _mm256_load_si256((const __mm256i*)src + 1);

	uint8_t from = mfrom(m);
	uint8_t to = mto(m);

	uint8_t reach = opposite_src[from];
	__m256i broadcast = _mm256_set1_epi8(src[from]);

	__m256i mask0 = move_reach_masks[from][reach][0];
	__m256i mask1 = move_reach_masks[from][reach][1];

	vector0 = _mm256_blendv_epi8(vector0, broadcast, mask0);
	vector1 = _mm256_blendv_epi8(vector1, broadcast, mask1);

	reach = opposite_src[to];
	broadcast = _mm256_set1_epi8(to);

	__m256i mask0 = move_reach_masks[to][reach][0];
	__m256i mask1 = move_reach_masks[to][reach][1];

	vector0 = _mm256_blendv_epi8(vector0, broadcast, mask0);
	vector1 = _mm256_blendv_epi8(vector1, broadcast, mask1);

	vector0 = _mm256_permute4x64_epi64(vector0, _MM_SHUFFLE(0, 1, 2, 3));
	vector1 = _mm256_permute4x64_epi64(vector1, _MM_SHUFFLE(0, 1, 2, 3));
	mask0 = _mm256_set1_epi8(0x38);

	vector0 = _mm256_xor_si256(vector0, mask);
	vector1 = _mm256_xor_si256(vector1, mask);

	_mm256_store_si256((__m256i*)dst, vector1);
	_mm256_store_si256(((__m256i*)dst) + 1, vector0);
}

#else

static inline void
update_reach_table(uint8_t *restrict dst,
			const uint8_t *restrict src,
			const uint8_t *restrict opposite_src,
    			move m)
{
	for (int i = 0; i < 64; i += 8) {
		for (int j = 0; j < 8; ++j)
			dst[i + (56 - i) + j] = flip_i(src[i + j]);
	}

	int from = mfrom(m);
	int i = opposite_src[from];
	from = flip_i(from);
	i = flip_i(i);

	uint8_t reach = dst[from];
	for (; i != from; i += dir)
		dst[i] = reach;

	int to = mto(m);
	int i = opposite_src[to];
	for (; i != to; i += dir)
		dst[i] = to;
}

#endif
