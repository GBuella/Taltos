cmake_minimum_required(VERSION 2.8)
project(taltos C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSLIDING_BYTE_LOOKUP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_KNIGHT_LOOKUP_TABLE")

include(CheckCCompilerFlag)
check_c_compiler_flag(-Wall HAS_WALL)
if (HAS_WALL)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()
check_c_compiler_flag(-Wextra HAS_WEXTRA)
if (HAS_WEXTRA)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
endif()
check_c_compiler_flag(-pedantic HAS_PEDANTIC)
if (HAS_PEDANTIC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
endif()
check_c_compiler_flag(-Werror HAS_WERROR)
if (HAS_WERROR)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()
check_c_compiler_flag(-std=c99 HAS_STDC99)
if (HAS_STDC99)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()
check_c_compiler_flag(-mpopcnt HAS_MPOPCNT)
if (HAS_MPOPCNT)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mpopcnt")
endif()
check_c_compiler_flag(-march=native HAS_MARCHNATIVE)
if (HAS_MARCHNATIVE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif()
FIND_PACKAGE(Threads)

include_directories(src)

add_executable(gen_constants util/gen_constants.c)

add_custom_command(
    OUTPUT  src/move_gen_const.inc
    COMMAND gen_constants > src/move_gen_const.inc
    DEPENDS gen_constants)

add_executable(taltos
    src/move_gen_const.inc
    src/book.c
    src/chess.c
    src/engine.c
    src/eval.c
    src/fen_book.c
    src/game.c
    src/hash.c
    src/main.c
    src/move.c
    src/move_gen.c
    src/move_order.c
    src/perft.c
    src/platform.c
    src/polyglotbook.c
    src/protocol.c
    src/search.c
    src/str_util.c
    src/tests.c
    src/timers.c
    src/trace.c)

TARGET_LINK_LIBRARIES(taltos ${CMAKE_THREAD_LIBS_INIT})

