enable_testing()

add_library(tests_main STATIC tests/test_main.c)

add_executable(test_bitboards tests/bitboards.c)
target_link_libraries(test_bitboards tests_main taltos_code)
add_test(NAME bitboards COMMAND $<TARGET_FILE:test_bitboards>)

add_executable(test_hash_table tests/hash_table.c)
target_link_libraries(test_hash_table tests_main taltos_code)
add_test(NAME hash_table COMMAND $<TARGET_FILE:test_hash_table>)

add_executable(test_strings tests/strings.c)
target_link_libraries(test_strings tests_main taltos_code)
add_test(NAME strings COMMAND $<TARGET_FILE:test_strings>)

add_executable(test_polyglot_key tests/polyglot_key.c)
target_link_libraries(test_polyglot_key tests_main taltos_code)
add_test(NAME polyglot_key COMMAND $<TARGET_FILE:test_polyglot_key>)

add_executable(test_game tests/game.c)
target_link_libraries(test_game tests_main taltos_code)
add_test(NAME game COMMAND $<TARGET_FILE:test_game>)

add_executable(test_move_order tests/move_order.c)
target_link_libraries(test_move_order tests_main taltos_code)

add_executable(test_eval_symmetry tests/eval_symmetry.c)
target_link_libraries(test_eval_symmetry tests_main taltos_code)

add_executable(test_zhash_key tests/zhash_key.c)
target_link_libraries(test_zhash_key tests_main taltos_code)

include(tests/perftsuite/CMakeLists.txt)
include(tests/perftsuite/CMakeLists_move_order.txt)
include(tests/perftsuite/CMakeLists_eval_symmetry.txt)
include(tests/perftsuite/CMakeLists_zhash_key.txt)
include(tests/positions/CMakeLists.txt)
