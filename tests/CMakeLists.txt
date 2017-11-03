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

add_library(tests_main STATIC test_main.c)

add_executable(test_bitboards bitboards.c)
target_link_libraries(test_bitboards tests_main taltos_code)
add_test(NAME bitboards COMMAND $<TARGET_FILE:test_bitboards>)

add_executable(test_hash_table hash_table.c)
target_link_libraries(test_hash_table tests_main taltos_code)
add_test(NAME hash_table COMMAND $<TARGET_FILE:test_hash_table>)

add_executable(test_strings strings.c)
target_link_libraries(test_strings tests_main taltos_code)
add_test(NAME strings COMMAND $<TARGET_FILE:test_strings>)

add_executable(test_polyglot_key polyglot_key.c)
target_link_libraries(test_polyglot_key tests_main taltos_code)
add_test(NAME polyglot_key COMMAND $<TARGET_FILE:test_polyglot_key>)

add_executable(test_game game.c)
target_link_libraries(test_game tests_main taltos_code)
add_test(NAME game COMMAND $<TARGET_FILE:test_game>)

add_executable(test_move_order move_order.c)
target_link_libraries(test_move_order tests_main taltos_code)

add_executable(test_eval_symmetry eval_symmetry.c)
target_link_libraries(test_eval_symmetry tests_main taltos_code)

add_executable(test_memcmp memcmp.c)
target_link_libraries(test_memcmp tests_main taltos_code)

include(perftsuite/CMakeLists.txt)
include(perftsuite/CMakeLists_move_order.txt)
include(perftsuite/CMakeLists_eval_symmetry.txt)
include(perftsuite/CMakeLists_memcmp.txt)
include(positions/CMakeLists.txt)

set_property(TEST perft_000 PROPERTY LABELS long)
set_property(TEST perft_001 PROPERTY LABELS long)
set_property(TEST perft_012 PROPERTY LABELS long)
set_property(TEST perft_013 PROPERTY LABELS long)
set_property(TEST perft_014 PROPERTY LABELS long)
set_property(TEST perft_015 PROPERTY LABELS long)
set_property(TEST perft_016 PROPERTY LABELS long)
set_property(TEST perft_017 PROPERTY LABELS long)
set_property(TEST perft_018 PROPERTY LABELS long)
set_property(TEST perft_029 PROPERTY LABELS long)
set_property(TEST perft_030 PROPERTY LABELS long)
set_property(TEST perft_031 PROPERTY LABELS long)
set_property(TEST perft_032 PROPERTY LABELS long)
set_property(TEST perft_033 PROPERTY LABELS long)
set_property(TEST perft_034 PROPERTY LABELS long)
set_property(TEST perft_035 PROPERTY LABELS long)
set_property(TEST perft_055 PROPERTY LABELS long)
set_property(TEST perft_057 PROPERTY LABELS long)
set_property(TEST memcmp_perft_001 PROPERTY LABELS long)
set_property(TEST memcmp_perft_126 PROPERTY LABELS long)
set_property(TEST eval_symmetry_perft_001 PROPERTY LABELS long)
set_property(TEST eval_symmetry_perft_126 PROPERTY LABELS long)
set_property(TEST position_zugzwang_1 PROPERTY LABELS long)
set_property(TEST position_zugzwang_2 PROPERTY LABELS long)
