enable_testing()

add_library(tests_main STATIC tests/test_main.c)

add_executable(test_bitboards tests/bitboards.c)
target_link_libraries(test_bitboards taltos_code tests_main)
add_test(NAME bitboards COMMAND $<TARGET_FILE:test_bitboards>)

add_executable(test_hash_table tests/hash_table.c)
target_link_libraries(test_hash_table taltos_code tests_main)
add_test(NAME hash_table COMMAND $<TARGET_FILE:test_hash_table>)

add_executable(test_strings tests/strings.c)
target_link_libraries(test_strings taltos_code tests_main)
add_test(NAME strings COMMAND $<TARGET_FILE:test_strings>)

add_executable(test_move tests/move.c)
target_link_libraries(test_move taltos_code tests_main)
add_test(NAME move COMMAND $<TARGET_FILE:test_move>)

add_executable(test_polyglot_key tests/polyglot_key.c)
target_link_libraries(test_polyglot_key taltos_code tests_main)
add_test(NAME polyglot_key COMMAND $<TARGET_FILE:test_polyglot_key>)

add_executable(test_game tests/game.c)
target_link_libraries(test_game taltos_code tests_main)
add_test(NAME game COMMAND $<TARGET_FILE:test_game>)

include(tests/perftsuite/CMakeLists.txt)
include(tests/positions/CMakeLists.txt)
