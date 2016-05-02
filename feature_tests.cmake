
include(CheckCSourceCompiles)
include(CheckCSourceRuns)

CHECK_C_SOURCE_COMPILES("
#include <sys/stat.h>
int main()
{
struct stat dummy;
fstat(1, &dummy);
return 0;
}
"
 TALTOS_CAN_USE_POSIX_FSTAT)

CHECK_C_SOURCE_COMPILES("
#include <Windows.h>
int main()
{
HANDLE file;
LARGE_INTEGER dummy;
GetFileSizeEx(file, &dummy);
return 0;
}
"
 TALTOS_CAN_USE_W_GETFILESIZEEX)


CHECK_C_SOURCE_COMPILES("
void *something(void) __attribute__((leaf));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_LEAF)

CHECK_C_SOURCE_COMPILES("
void *something(void) __attribute__((malloc));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_MALLOC)

CHECK_C_SOURCE_COMPILES("
void *something(long) __attribute__((alloc_size(1)));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_ALLOC_SIZE)

CHECK_C_SOURCE_COMPILES("
int something(int) __attribute__((const));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_CONST)

CHECK_C_SOURCE_COMPILES("
int something(int) __attribute__((pure));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_PURE)

CHECK_C_SOURCE_COMPILES("
int something(const char*, ...) __attribute__((format(printf, 1, 2)));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_PRINTF)

CHECK_C_SOURCE_COMPILES("
int something(const char*, void*) __attribute__((nonnull));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_NONNULL)

CHECK_C_SOURCE_COMPILES("
void *something(const char*, void*) __attribute__((returns_nonnull));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_RET_NONNULL)

CHECK_C_SOURCE_COMPILES("
void *something(const char*, void*) __attribute__((warn_unused_result));
int main() {}
"
 TALTOS_CAN_USE_ATTRIBUTE_WARN_UNUSED_RESULT)

CHECK_C_SOURCE_COMPILES("
void *something(const char*p)
{
	__builtin_prefetch(p, 1);
	return 0;
}
int main() {}
"
 TALTOS_CAN_USE_BUILTIN_PREFETCH)

