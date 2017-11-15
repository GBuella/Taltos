# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4: #
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

cmake_minimum_required(VERSION 3.8)
project(taltos CXX)

FIND_PACKAGE(Threads)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
# this should not be needed statring with cmake 3.10
	add_compile_options("/std:c++17")
endif()

include_directories(src constants tests "${PROJECT_BINARY_DIR}")

add_executable(epd_runner EXCLUDE_FROM_ALL tools/epd_runner.cc)
add_executable(gen_SEE_table EXCLUDE_FROM_ALL tools/gen_SEE_table.cc)
add_executable(gen_bb_constants EXCLUDE_FROM_ALL tools/gen_bb_constants.cc)
add_executable(pdump EXCLUDE_FROM_ALL tools/pdump.cc)
add_executable(bitboard EXCLUDE_FROM_ALL tools/bitboard.cc)
add_executable(cmp_text tools/cmp_text.cc)

option(TALTOS_WARN_ME_HARD "Enable warnings" OFF)
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

include(cmake/feature_tests.cmake)
if(AUTO_CTAGS)
	include(cmake/ctags.cmake)
endif()

set(TALTOS_SOURCES
	src/eval.cc
	src/fen.cc
	src/game.cc
	src/move.cc
	src/move_desc.cc
	src/move_gen.cc
	src/move_order.cc
	src/perft.cc
	src/position.cc
	src/str_util.cc
	src/util.cc
	src/trace.cc
	src/tt.cc)

add_library(taltos_code STATIC ${TALTOS_SOURCES})

add_executable(taltos src/main.cc src/engine.cc src/command_loop.cc src/search.cc)
target_link_libraries(taltos taltos_code)

target_link_libraries(pdump taltos_code)

if(TALTOS_CAN_USE_LOG2_WITH_LIBM)
	target_link_libraries(taltos m)
endif()

configure_file(cmake/config.h.in taltos_config.h)

TARGET_LINK_LIBRARIES(taltos ${CMAKE_THREAD_LIBS_INIT})

if(TALTOS_BUILD_TESTS)
enable_testing()
add_subdirectory(tests)
endif()
