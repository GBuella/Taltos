# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4:
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

CHECK_INCLUDE_FILES(Windows.h TALTOS_CAN_USE_WINDOWS_H)

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

CHECK_FUNCTION_EXISTS(clock_gettime TALTOS_CAN_USE_CLOCK_GETTIME)
if(TALTOS_CAN_USE_WINDOWS_H AND NOT TALTOS_CAN_USE_CLOCK_GETTIME)
	CHECK_FUNCTION_EXISTS(QueryPerformanceFrequency
			TALTOS_CAN_USE_W_PERFCOUNTER)
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
