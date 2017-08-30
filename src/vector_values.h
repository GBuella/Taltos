
/* vim: set filetype=c : */
/* vim: set noet ts=8 sw=8 cinoptions=+4,(4: */

#include "taltos_config.h"

#if defined(USE_GLOBAL_REGISTERS) \
	&& defined(TALTOS_CAN_USE_IMMINTRIN_H) \
	&& !defined(VECTOR_VALUES_H)
#define VECTOR_VALUES_H

#include <immintrin.h>

#pragma GCC system_header

#if defined(TALTOS_CAN_USE_INTEL_AVX2) \
	&& defined(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_YMM)

register __m256i bitboard_flip_shufflekey_32 asm("ymm6");
register __m256i ymm_zero asm("ymm7");

#define HAS_YMM_ZERO

#elif defined(TALTOS_CAN_USE_INTEL_AVX) \
	&& defined(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_XMM)

register __m128i bitboard_flip_shufflekey_16 asm("xmm6");

#endif


#endif
