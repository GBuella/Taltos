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

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

if(TALTOS_WARN_ME_HARD)
	check_cxx_compiler_flag(-Werror HAS_WERROR)
	check_cxx_compiler_flag(-Wall HAS_WALL)
	check_cxx_compiler_flag(-Wextra HAS_WEXTRA)
	check_cxx_compiler_flag(-pedantic HAS_PEDANTIC)
	check_cxx_compiler_flag(-Wno-format HAS_NOFORMAT)
endif()

check_cxx_compiler_flag(-march=native HAS_MARCHNATIVE)
check_cxx_compiler_flag(-mavx HAS_MAVX_FLAG)
check_cxx_compiler_flag(-wd188 HAS_WD188)
if(HAS_MAVX_FLAG)
	check_cxx_compiler_flag(-mavx2 HAS_MAVX2_FLAG)
endif()

if(NOT TALTOS_FORCE_NO_BUILTINS)
	check_cxx_compiler_flag(/Oi HAS_OI_FLAG)
	check_cxx_compiler_flag(/arch:AVX HAS_ARCH_AVX_FLAG)
	if(HAS_ARCH_AVX_FLAG)
		check_cxx_compiler_flag(/arch:AVX2 HAS_ARCH_AVX2_FLAG)
	endif()
endif()


# These must be set before doing feature tests for intrinsics
if(HAS_MARCHNATIVE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

if(HAS_OI_FLAG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Oi")
endif()

if(HAS_WD188)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -wd188")
endif()

if(TALTOS_FORCE_NO_AVX)
	set(TALTOS_FORCE_NO_AVX2 ON)
endif()

if(TALTOS_FORCE_AVX2)
	if(HAS_MAVX2_FLAG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
	endif()
	if(HAS_ARCH_AVX2_FLAG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
	endif()
else()
	if(TALTOS_FORCE_AVX)
		if(HAS_MAVX_FLAG)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
		endif()
		if(HAS_ARCH_AVX_FLAG)
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
		endif()
	endif()
endif()

# Warnings must be set before doing feature tests for intrinsics/builtins
#  some of those would fail due to warnings

if(HAS_WERROR)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()
if(HAS_WALL)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()
if(HAS_WEXTRA)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
endif()
if(HAS_PEDANTIC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
endif()

CHECK_CXX_SOURCE_COMPILES("
#include <cmath>
int main() {
	double x = 3.0;
	double y = std::log2(x);
	(void) y;
	return 0;
}
"
 TALTOS_CAN_USE_LOG2_WITHOUT_LIBM)

if(NOT TALTOS_CAN_USE_LOG2_WITHOUT_LIBM)
set(orig_req_libs ${CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} m)

CHECK_CXX_SOURCE_COMPILES("
#include <cmath>
int main() {
	double x = 3.0;
	double y = std::log2(x);
	(void) y;
	return 0;
}
"
 TALTOS_CAN_USE_LOG2_WITH_LIBM)

if(NOT TALTOS_CAN_USE_LOG2_WITH_LIBM)
	message(FATAL_ERROR "Unable to use log2 from cmath")
endif()

set(CMAKE_REQUIRED_LIBRARIES ${orig_req_libs})
endif()

CHECK_CXX_SOURCE_COMPILES("
int something(const char*, ...) __attribute__((format(printf, 1, 2)));
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_GNU_ATTRIBUTE_PRINTF)

CHECK_CXX_SOURCE_COMPILES("
int something(char *restrict a, char *restrict b);
int main() {
	return 0;
}
"
 TALTOS_CAN_USE_RESTRICT_KEYWORD)

CHECK_CXX_SOURCE_COMPILES("
int something(char *__restrict a, char *__restrict b);
int main() {
	return 0;
}
"
 TALTOS_CAN_USE___RESTRICT_KEYWORD)
