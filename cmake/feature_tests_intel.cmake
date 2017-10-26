#
# Copyright 2014-2017, Gabor Buella
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if(NOT TALTOS_FORCE_NO_BUILTINS)

# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4:
#
# Intel intrinsics
# https://software.intel.com/sites/landingpage/IntrinsicsGuide

include(CheckCSourceCompiles)
include(CheckIncludeFiles)

CHECK_INCLUDE_FILES(immintrin.h TALTOS_CAN_USE_IMMINTRIN_H)

if(TALTOS_CAN_USE_IMMINTRIN_H)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
uint64_t something(uint64_t x)
{
    return _bswap64(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_BSWAP64)

if(NOT TALTOS_CAN_USE_BUILTIN_CTZL_64
        AND NOT TALTOS_CAN_USE_BUILTIN_CTZLL_64)
CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
int something(uint64_t x)
{
	unsigned long index;

	_BitScanForward64(&index, x);
	result = (int)index;
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_BITSCANFORWARD64)
endif()

if(NOT TALTOS_CAN_USE_BUILTIN_CLZL_64
        AND NOT TALTOS_CAN_USE_BUILTIN_CLZLL_64)
CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
int something(uint64_t x)
{
	unsigned long index;

	_BitScanReverse64(&index, x);
	result = (int)index;
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_BITSCANREVERSE64)
endif()

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
uint64_t something(uint64_t x)
{
	return _blsi_u64(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_BLSI64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
uint64_t something(uint64_t x)
{
	return _blsr_u64(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_BLSR64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
int something(uint64_t x)
{
    return _mm_popcnt_u64(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_POPCOUNT64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
#include <immintrin.h>
unsigned something(uint64_t x)
{
    return _pdep_u64(x, x) + _pext_u64(x, x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_PDEP_PEXT_64)

CHECK_C_SOURCE_COMPILES("
#include <xmmintrin.h>
void *something(int x) { return _mm_malloc(64, x); }
void other(void *x) { _mm_free(x); }
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_MMALLOC)

CHECK_C_SOURCE_COMPILES("
#include <immintrin.h>
__m128i something(__m128i x) { return _mm_shuffle_epi8(x, x); }
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_SHUFFLE_EPI8)

CHECK_C_SOURCE_COMPILES("
#include <immintrin.h>
__m128i something(__m128i x) { return _mm_shuffle_epi32(x, 1); }
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_SHUFFLE_EPI32)

if(TALTOS_FORCE_AVX)
	set(TALTOS_CAN_USE_INTEL_AVX ON)
else()
if(NOT TALTOS_FORCE_NO_AVX)
CHECK_C_SOURCE_COMPILES("
#include <immintrin.h>
__m256d something(__m256d x) { return _mm256_permute_pd(x, 6); }
__m256d other(__m256d x) { return _mm256_permute2f128_pd(x, x, 1); }
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_AVX)
endif()
endif()

if(TALTOS_FORCE_AVX2)
	set(TALTOS_CAN_USE_INTEL_AVX2 ON)
else()
if(TALTOS_CAN_USE_INTEL_AVX)
CHECK_C_SOURCE_COMPILES("
#include <immintrin.h>
__m256i something(__m256i x) { return _mm256_shuffle_epi8(x, x); }
__m256d other(__m256d x) { return _mm256_permute4x64_pd(x, 13); }
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_INTEL_AVX2)
endif()
endif()

# endif TALTOS_CAN_USE_IMMINTRIN_H
endif()

endif()
