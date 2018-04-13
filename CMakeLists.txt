# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4: #
#
# Copyright 2014-2018, Gabor Buella
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

cmake_minimum_required(VERSION 3.0)
project(taltos C)

FIND_PACKAGE(Threads)

include_directories(src constants tests "${PROJECT_BINARY_DIR}")

add_executable(epd_runner EXCLUDE_FROM_ALL tools/epd_runner.c)
add_executable(gen_SEE_table EXCLUDE_FROM_ALL tools/gen_SEE_table.c)
add_executable(gen_bb_constants EXCLUDE_FROM_ALL tools/gen_bb_constants.c)
add_executable(pdump EXCLUDE_FROM_ALL tools/pdump.c)
add_executable(bitboard EXCLUDE_FROM_ALL tools/bitboard.c)
add_executable(cmp_text tools/cmp_text.c)

option(TALTOS_FORCE_NO_BUILTINS "Do not use builtin intrinsics" OFF)
option(TALTOS_FORCE_AVX "Force using AVX builtin intrinsics" OFF)
option(TALTOS_FORCE_AVX2 "Force using AVX2 builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_AVX "Do not use AVX builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_AVX2 "Do not use AVX2 builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_SSE "Do not use SSE builtin intrinsics" OFF)
option(TALTOS_BUILD_TESTS "Build tests" ON)
option(AUTO_CTAGS "Run ctags automatically" OFF)

if(TALTOS_FORCE_AVX AND TALTOS_FORCE_NO_AVX)
	message(FATAL_ERROR "I'm sorry Dave, I'm afraid I can't do that.")
endif()
if(TALTOS_FORCE_AVX2 AND TALTOS_FORCE_NO_AVX2)
	message(FATAL_ERROR "I'm sorry Dave, I'm afraid I can't do that.")
endif()
if(TALTOS_FORCE_AVX2 AND TALTOS_FORCE_NO_AVX)
	message(FATAL_ERROR "I'm sorry Dave, I'm afraid I can't do that.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(cmake/feature_tests.cmake)
if(AUTO_CTAGS)
	include(cmake/ctags.cmake)
endif()

set(TALTOS_SOURCES
	src/book.c
	src/eval.c
	src/fen.c
	src/fen_book.c
	src/game.c
	src/tt.c
	src/move.c
	src/move_desc.c
	src/move_gen.c
	src/move_order.c
	src/perft.c
	src/polyglot_book.c
	src/position.c
	src/str_util.c
	src/util.c
	src/trace.c
	src/zobrist.c)

set_source_files_properties(src/position.c PROPERTIES COMPILE_FLAGS -fno-builtin-memcpy)

if(NOT TALTOS_CAN_USE_ISO_THREADS)
	set(TALTOS_SOURCES tinycthread/tinycthread.c ${TALTOS_SOURCES})
	include_directories(src tests tinycthread "${PROJECT_BINARY_DIR}")
endif()

add_library(taltos_code STATIC ${TALTOS_SOURCES})

add_executable(taltos src/main.c src/engine.c src/command_loop.c src/search.c)
target_link_libraries(taltos taltos_code)

target_link_libraries(pdump taltos_code)

if(TALTOS_CAN_USE_LOG2_WITH_LIBM)
	target_link_libraries(taltos m)
endif()

if(HAS_FLAX_VCONVS)
	target_compile_options(taltos PUBLIC -flax-vector-conversions)
endif()
if(HAS_MNO_VZEROUPPER)
	target_compile_options(taltos_code PUBLIC -mno-vzeroupper)
endif()

# hack: this gets rid of some declarations in Visual C++ headers
if(MSVC)
	set(CMAKE_C_FLAGS
	    "${CMAKE_C_FLAGS} -D__STDC__ -D_CRT_SECURE_NO_DEPRECATE")
endif()

configure_file(cmake/config.h.in taltos_config.h)

TARGET_LINK_LIBRARIES(taltos ${CMAKE_THREAD_LIBS_INIT})

if(TALTOS_BUILD_TESTS)
enable_testing()
add_subdirectory(tests)
endif()
