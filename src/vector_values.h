
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

#define HAS_YMM_ZERO
#define HAS_XMM_SHUFFLE_CONTROL_MASK_32

#elif defined(TALTOS_CAN_USE_INTEL_AVX) \
	&& defined(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_YMM) \
	&& defined(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_XMM)

#define HAS_YMM_ZERO
#define HAS_XMM_SHUFFLE_CONTROL_MASK_16

#elif defined(TALTOS_CAN_USE_INTEL_SHUFFLE_EPI8) \
	&& defined(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_XMM)

#define HAS_XMM_SHUFFLE_CONTROL_MASK_16

#endif


#endif
