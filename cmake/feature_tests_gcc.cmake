
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
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_GNU_ATTRIBUTE_SYNTAX)

if(NOT TALTOS_FORCE_NO_BUILTINS)

CHECK_C_SOURCE_COMPILES("
int something(const char*p)
{
    if (p == 0) __builtin_unreachable();
	return 0;
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_BUILTIN_UNREACHABLE)

CHECK_C_SOURCE_COMPILES("
int something(const char*p)
{
	__builtin_prefetch(p, 1);
	return 0;
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_BUILTIN_PREFETCH)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long) == sizeof(uint64_t), \"nope\");
int something(unsigned long x)
{
	return __builtin_ctzl(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_BUILTIN_CTZL_64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long long) == sizeof(uint64_t), \"nope\");
int something(unsigned long long x)
{
	return __builtin_ctzll(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_BUILTIN_CTZLL_64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
_Static_assert(sizeof(long) == sizeof(uint64_t), \"nope\");
int something(unsigned long x)
{
	return __builtin_clzl(x);
}
int main() {
	return 0;
}
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
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_BUILTIN_POPCOUNTLL64)

CHECK_C_SOURCE_COMPILES("
#include <stdint.h>
uint64_t something(uint64_t x)
{
    return __builtin_bswap64(x);
}
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_BUILTIN_BSWAP64)

check_c_compiler_flag(-flax-vector-conversions HAS_FLAX_VCONVS)
check_c_compiler_flag(-mno-vzeroupper HAS_MNO_VZEROUPPER)

endif()
