# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4: #
cmake_minimum_required(VERSION 2.8)
project(taltos C)

FIND_PACKAGE(Threads)

include_directories(src tests "${PROJECT_BINARY_DIR}")

add_executable(gen_constants EXCLUDE_FROM_ALL tools/gen_constants.c)
add_executable(epd_runner tools/epd_runner.c)
add_executable(cmp_text tools/cmp_text.c)

option(TALTOS_FORCE_NO_BUILTINS "Do not use builtin intrinsics" OFF)
option(TALTOS_FORCE_AVX "Force using AVX builtin intrinsics" OFF)
option(TALTOS_FORCE_AVX2 "Force using AVX2 builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_AVX "Do not use AVX builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_AVX2 "Do not use AVX2 builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_SSE "Do not use SSE builtin intrinsics" OFF)

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

set(TALTOS_SOURCES
	src/book.c
	src/constants.c
	src/eval.c
	src/fen.c
	src/fen_book.c
	src/game.c
	src/hash.c
	src/move.c
	src/move_gen.c
	src/move_order.c
	src/perft.c
	src/polyglot_book.c
	src/position.c
	src/str_util.c
	src/util.c
	src/trace.c
	src/zobrist.c)

if(NOT TALTOS_CAN_USE_ISO_THREADS)
	set(TALTOS_SOURCES tinycthread/tinycthread.c ${TALTOS_SOURCES})
	include_directories(src tests tinycthread "${PROJECT_BINARY_DIR}")
endif()

add_library(taltos_code STATIC ${TALTOS_SOURCES})

add_executable(taltos src/main.c src/engine.c src/command_loop.c src/search.c)
target_link_libraries(taltos taltos_code)

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

add_custom_target(cstyle
	COMMAND perl ${PROJECT_SOURCE_DIR}/tools/cstyle.pl
	-pc ${PROJECT_SOURCE_DIR}/src/*
	${PROJECT_SOURCE_DIR}/tests/*.{h,c})

enable_testing()
add_subdirectory(tests)
