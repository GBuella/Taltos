
include(CheckCCompilerFlag)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckCSourceCompiles)

check_c_compiler_flag(-Werror HAS_WERROR)
check_c_compiler_flag(-Wall HAS_WALL)
check_c_compiler_flag(-Wextra HAS_WEXTRA)
check_c_compiler_flag(-pedantic HAS_PEDANTIC)
check_c_compiler_flag(-march=native HAS_MARCHNATIVE)
check_c_compiler_flag(-Wunknown-attributes HAS_WUNKNOWN_ATTR_FLAG)
check_c_compiler_flag(-Wattributes HAS_WATTR_FLAG)


if (HAS_MARCHNATIVE)
#   This must be set before doing feature tests for intel intrinsics
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif()



# fstat or GetFileSizeEx is used to find out the size of a file
CHECK_INCLUDE_FILES(sys/stat.h TALTOS_CAN_USE_SYS_STAT_H)
if (TALTOS_CAN_USE_SYS_STAT_H)
    CHECK_FUNCTION_EXISTS(fstat TALTOS_CAN_USE_POSIX_FSTAT)
endif()

if (NOT TALTOS_CAN_USE_POSIX_FSTAT)
    CHECK_INCLUDE_FILES(Windows.h TALTOS_CAN_USE_WINDOWS_H)
endif()

if (TALTOS_CAN_USE_WINDOWS_H AND NOT TALTOS_CAN_USE_POSIX_FSTAT)
    CHECK_FUNCTION_EXISTS(GetFileSizeEx TALTOS_CAN_USE_W_GETFILESIZEEX)
endif()

CHECK_FUNCTION_EXISTS(getrusage TALTOS_CAN_USE_GETRUSAGE)


include(cmake/feature_tests_gcc.cmake)
include(cmake/feature_tests_intel.cmake)



# Warnings must be set before doing feature tests for intrinsics/builtins
#  some of those would fail due to warnings

if (HAS_WERROR)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()
if (HAS_WALL)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()
if (HAS_WEXTRA)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
endif()
if (HAS_PEDANTIC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
endif()

if (TALTOS_CAN_USE_GNU_ATTRIBUTE_SYNTAX)
    if (HAS_WUNKNOWN_ATTR_FLAG)
#       used in clang
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-attributes")
    else()
        if (HAS_WATTR_FLAG)
#           used in GCC
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
int main() {}
"
 TALTOS_CAN_USE_ISO_ALIGNAD_ALLOC)

CHECK_C_SOURCE_COMPILES("
#ifdef __STDC_NO_THREADS__
#error no threads!
#endif
#include <threads.h>

int main() {}
(void) thrd_current();
"
 TALTOS_CAN_USE_ISO_THREADS)

set(CMAKE_REQUIRED_FLAGS ${orig_req_flags})

CHECK_FUNCTION_EXISTS(mach_absolute_time TALTOS_CAN_USE_MACH_ABS_TIME)
if (NOT TALTOS_CAN_USE_MACH_ABS_TIME)
    CHECK_FUNCTION_EXISTS(clock_gettime TALTOS_CAN_USE_CLOCK_GETTIME)
endif()
