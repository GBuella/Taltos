# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4:

include(CheckCCompilerFlag)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckCSourceCompiles)

check_c_compiler_flag(-std=c11 HAS_STDC11)
if (HAS_STDC11)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
else()
	check_c_compiler_flag(-std=gnu11 HAS_GNU11)
	if (HAS_GNU11)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
	endif()
endif()

check_c_compiler_flag(-Werror HAS_WERROR)
check_c_compiler_flag(-Wall HAS_WALL)
check_c_compiler_flag(-Wextra HAS_WEXTRA)
check_c_compiler_flag(-pedantic HAS_PEDANTIC)
check_c_compiler_flag(-Wno-format HAS_NOFORMAT)
check_c_compiler_flag(-march=native HAS_MARCHNATIVE)
check_c_compiler_flag(-Wunknown-attributes HAS_WUNKNOWN_ATTR_FLAG)
check_c_compiler_flag(-Wattributes HAS_WATTR_FLAG)
check_c_compiler_flag(-mavx HAS_MAVX_FLAG)
check_c_compiler_flag(-wd188 HAS_WD188)
if(HAS_MAVX_FLAG)
	check_c_compiler_flag(-mavx2 HAS_MAVX2_FLAG)
endif()

if(NOT TALTOS_FORCE_NO_BUILTINS)
	check_c_compiler_flag(/Oi HAS_OI_FLAG)
	check_c_compiler_flag(/arch:AVX HAS_ARCH_AVX_FLAG)
	if(HAS_ARCH_AVX_FLAG)
		check_c_compiler_flag(/arch:AVX2 HAS_ARCH_AVX2_FLAG)
	endif()
endif()


# These must be set before doing feature tests for intrinsics
if(HAS_MARCHNATIVE)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif()

if(HAS_OI_FLAG)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Oi")
endif()

if(HAS_WD188)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -wd188")
endif()

if(TALTOS_FORCE_NO_AVX)
	set(TALTOS_FORCE_NO_AVX2 ON)
endif()

if(TALTOS_FORCE_AVX2)
	if(HAS_MAVX2_FLAG)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2")
	endif()
	if(HAS_ARCH_AVX2_FLAG)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX2")
	endif()
else()
	if(TALTOS_FORCE_AVX)
		if(HAS_MAVX_FLAG)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx")
		endif()
		if(HAS_ARCH_AVX_FLAG)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:AVX")
		endif()
	endif()
endif()

# fstat or GetFileSizeEx is used to find out the size of a file
CHECK_INCLUDE_FILES(sys/stat.h TALTOS_CAN_USE_SYS_STAT_H)
if(TALTOS_CAN_USE_SYS_STAT_H)
	CHECK_FUNCTION_EXISTS(fstat TALTOS_CAN_USE_POSIX_FSTAT)
endif()

CHECK_INCLUDE_FILES(Windows.h TALTOS_CAN_USE_WINDOWS_H)

if(TALTOS_CAN_USE_WINDOWS_H)
	CHECK_FUNCTION_EXISTS(_filelengthi64 TALTOS_CAN_USE_W_FILELENGTHI64)
endif()

CHECK_FUNCTION_EXISTS(getrusage TALTOS_CAN_USE_GETRUSAGE)


include(cmake/feature_tests_gcc.cmake)
include(cmake/feature_tests_intel.cmake)



# Warnings must be set before doing feature tests for intrinsics/builtins
#  some of those would fail due to warnings

if(HAS_WERROR)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()
if(HAS_WALL)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()
if(HAS_WEXTRA)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
endif()
if(HAS_PEDANTIC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
endif()

if(TALTOS_CAN_USE_GNU_ATTRIBUTE_SYNTAX)
	if(HAS_WUNKNOWN_ATTR_FLAG)
#       	used in clang
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-attributes")
	else()
		if(HAS_WATTR_FLAG)
#			used in GCC
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-attributes")
		endif()
	endif()
endif()

set(orig_req_flags ${CMAKE_REQUIRED_FLAGS})

set(CMAKE_REQUIRED_FLAGS "${orig_req_flags} -std=gnu11")

# This is in ISO C since 2011,
# but is not present in a lot of libc
# implementations as of 2016
CHECK_C_SOURCE_COMPILES("
#include <stdlib.h>
void *something(int x) { return aligned_alloc(64, x); }
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_ISO_ALIGNAD_ALLOC)

CHECK_C_SOURCE_COMPILES("
#ifdef __STDC_NO_THREADS__
#error no threads!
#endif
#include <threads.h>

int main() {
	(void) thrd_current();
	return 0;
}
"
 TALTOS_CAN_USE_ISO_THREADS)

set(CMAKE_REQUIRED_FLAGS ${orig_req_flags})

CHECK_FUNCTION_EXISTS(mach_absolute_time TALTOS_CAN_USE_MACH_ABS_TIME)
if(NOT TALTOS_CAN_USE_MACH_ABS_TIME)
	CHECK_FUNCTION_EXISTS(clock_gettime TALTOS_CAN_USE_CLOCK_GETTIME)
	if(TALTOS_CAN_USE_WINDOWS_H AND NOT TALTOS_CAN_USE_CLOCK_GETTIME)
		CHECK_FUNCTION_EXISTS(QueryPerformanceFrequency
				TALTOS_CAN_USE_W_PERFCOUNTER)
	endif()
endif()

CHECK_C_SOURCE_COMPILES("
#include <math.h>
int main() {
	double x = 3.0;
	double y = log2(x);
	(void) y;
	return 0;
}
"
 TALTOS_CAN_USE_LOG2_WITHOUT_LIBM)

if(NOT TALTOS_CAN_USE_LOG2_WITHOUT_LIBM)
set(orig_req_libs ${CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} m)

CHECK_C_SOURCE_COMPILES("
#include <math.h>
int main() {
	double x = 3.0;
	double y = log2(x);
	(void) y;
	return 0;
}
"
 TALTOS_CAN_USE_LOG2_WITH_LIBM)

if(NOT TALTOS_CAN_USE_LOG2_WITH_LIBM)
	message(FATAL_ERROR "Unable to use log2 from math.h")
endif()

set(CMAKE_REQUIRED_LIBRARIES ${orig_req_libs})
endif()

if(HAS_PEDANTIC AND HAS_WERROR AND HAS_NOFORMAT)

CHECK_C_SOURCE_COMPILES("
#include <stdio.h>
#include <stdint.h>
int main(void) {
	intmax_t n = 123;
	(void) printf(\"%j\", n);
	return 0;
}
"
 TALTOS_CAN_USE_ISOC_FORMATS)

if(NOT TALTOS_CAN_USE_ISOC_FORMATS)

set(orig_req_flags "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "${orig_req_flags} -Wno-format")

CHECK_C_SOURCE_COMPILES("
#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdint.h>
int main(void) {
	intmax_t n = 123;
	(void) printf(\"%j\", n);
	return 0;
}
"
 TALTOS_CAN_USE_MINGW_ISOC_FORMATS)

set(CMAKE_REQUIRED_FLAGS ${orig_req_flags})

endif()

if(TALTOS_CAN_USE_MINGW_ISOC_FORMATS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__USE_MINGW_ANSI_STDIO")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format")
endif()

endif()
