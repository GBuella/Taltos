# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4: #
cmake_minimum_required(VERSION 2.8)
project(taltos C)

FIND_PACKAGE(Threads)

include_directories(src tests "${PROJECT_BINARY_DIR}")

add_executable(gen_constants tools/gen_constants.c)
add_executable(epd_runner tools/epd_runner.c)

# https://chessprogramming.wikispaces.com/Magic+Bitboards#Implementations-Byte%20Lookup
option(SLIDING_BYTE_LOOKUP "magic bitboards byte lookup" ON)
option(USE_KNIGHT_LOOKUP_TABLE "lookup table for knight moves" ON)
option(USE_GLOBAL_REGISTERS "Store some values in some XMM / YMM registers" OFF)
option(TALTOS_FORCE_NO_BUILTINS "Do not use builtin intrinsics" OFF)
option(TALTOS_FORCE_AVX "Force using AVX builtin intrinsics" OFF)
option(TALTOS_FORCE_AVX2 "Force using AVX2 builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_AVX "Do not use AVX builtin intrinsics" OFF)
option(TALTOS_FORCE_NO_AVX2 "Do not use AVX2 builtin intrinsics" OFF)

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

add_custom_command(
	OUTPUT  move_gen_const.inc
	COMMAND gen_constants > move_gen_const.inc
	DEPENDS gen_constants)

set(TALTOS_SOURCES
	src/book.c
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
	src/util.c)

if(NOT TALTOS_CAN_USE_ISO_THREADS)
	set(TALTOS_SOURCES tinycthread/tinycthread.c ${TALTOS_SOURCES})
	include_directories(src tests tinycthread "${PROJECT_BINARY_DIR}")
endif()

add_library(taltos_code STATIC move_gen_const.inc ${TALTOS_SOURCES})

add_executable(taltos src/main.c src/engine.c src/command_loop.c src/search.c)
target_link_libraries(taltos taltos_code)

set_property(TARGET taltos PROPERTY C_STANDARD 11)
# set_property(TARGET taltos PROPERTY C_EXTENSIONS OFF)
set_property(TARGET taltos PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET gen_constants PROPERTY C_STANDARD 11)
# set_property(TARGET gen_constants PROPERTY C_EXTENSIONS OFF)
set_property(TARGET gen_constants PROPERTY C_STANDARD_REQUIRED ON)


if(HAS_FLAX_VCONVS)
	target_compile_options(taltos PUBLIC -flax-vector-conversions)
endif()

# This is needed to instruct GCC not to clobber the SIMD
# registers used as permanent storage
#
if(TALTOS_CAN_USE_GCC_GLOBAL_REGISTER_VARIABLE_XMM AND USE_GLOBAL_REGISTERS)
#  if(HAS_MNO_VZEROUPPER)
#      target_compile_options(taltos PUBLIC -mno-vzeroupper)
#  endif()
	if(HAS_FIXED_REGISTER_FLAG)
		target_compile_options(taltos PUBLIC -ffixed-xmm6)
		target_compile_options(taltos PUBLIC -ffixed-xmm7)
	endif()
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

include(tests/CMakeLists.txt)
