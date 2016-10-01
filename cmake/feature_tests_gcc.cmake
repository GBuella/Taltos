
# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4:
#
# extensions in GCC-like compilers
# https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html

include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCCompilerFlag)

CHECK_C_SOURCE_COMPILES("
int something(const char*, ...) __attribute__((format(printf, 1, 2)));
int main() {}
"
 TALTOS_CAN_USE_GNU_ATTRIBUTE_SYNTAX)

if(TALTOS_CAN_USE_GNU_ATTRIBUTE_SYNTAX)
CHECK_C_SOURCE_RUNS("
static int x;
static __attribute__((constructor)) void some(void) {
    x = 7;
}
int main() {
    return (x == 7) ? 0 : 1;
}
"
 TALTOS_CAN_USE_CONSTRUCTOR_ATTRIBUTE)
endif()

if(NOT TALTOS_FORCE_NO_BUILTINS)

CHECK_C_SOURCE_COMPILES("
void *something(const char*p)
{
    if (p == 0) __builtin_unreachable();
	return 0;
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_UNREACHABLE)

CHECK_C_SOURCE_COMPILES("
void *something(const char*p)
{
	__builtin_prefetch(p, 1);
	return 0;
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_PREFETCH)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long) == sizeof(uint64_t), \"nope\");
int something(unsigned long x)
{
	return __builtin_ctzl(x);
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_CTZL_64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long long) == sizeof(uint64_t), \"nope\");
int something(unsigned long long x)
{
	return __builtin_ctzll(x);
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_CTZLL_64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long) == sizeof(uint64_t), \"nope\");
int something(unsigned long x)
{
	return __builtin_clzl(x);
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_CLZL_64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long long) == sizeof(uint64_t), \"nope\");
int something(unsigned long long x)
{
	return __builtin_clzll(x);
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_CLZLL_64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long long) == sizeof(uint64_t), \"nope\");
int something(unsigned long long x)
{
	return __builtin_popcountll(x);
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_POPCOUNTLL64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
uint64_t something(uint64_t x)
{
    return __builtin_bswap64(x);
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_BSWAP64)

# register char test_global_vector
# asm(\"xmm7\") __attribute__ ((__vector_size__(16)));
CHECK_C_SOURCE_COMPILES("
void something(void) {
    register volatile char test_var __asm__(\"xmm7\")
    __attribute__ ((__vector_size__(16)));

    test_var = (char __attribute__((__vector_size__(16)))){
                             15, 14, 13, 12, 11, 10,  9,  8,
                              7,  6,  5,  4,  3,  2,  1,  0};
}
int main() { return 0; }"
  TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_XMM)

if(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_XMM)

CHECK_C_SOURCE_COMPILES("
void something(void) {
    register volatile char test_var __asm__(\"xmm7\")
    __attribute__ ((__vector_size__(32)));

    test_var = (char __attribute__((__vector_size__(32)))){
                             15, 14, 13, 12, 11, 10,  9,  8,
                              7,  6,  5,  4,  3,  2,  1,  0,
                             15, 14, 13, 12, 11, 10,  9,  8,
                              7,  6,  5,  4,  3,  2,  1,  0};
}
int main() { return 0; }"
  TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_YMM)

check_c_compiler_flag(-flax-vector-conversions HAS_FLAX_VCONVS)
check_c_compiler_flag(-mno-vzeroupper HAS_MNO_VZEROUPPER)
check_c_compiler_flag(-ffixed-xmm7 HAS_FIXED_REGISTER_FLAG)

endif()

endif()
