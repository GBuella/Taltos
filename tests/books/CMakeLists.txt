
add_executable(gen_dummy_polyg_book gen_dummy_polyg_book.c)

add_custom_command(OUTPUT dummy_book_4.bin dummy_book_1.bin
		COMMAND gen_dummy_polyg_book dummy_book_4.bin dummy_book_1.bin
		DEPENDS gen_dummy_polyg_book)

add_custom_target(dummy_polyglot_books ALL DEPENDS dummy_book_4.bin dummy_book_1.bin)

add_test(NAME "no_book_argument"
	COMMAND ${CMAKE_COMMAND}
	-DTEST_PROG=$<TARGET_FILE:taltos>
	-DCMP_PROG=$<TARGET_FILE:cmp_text>
	-DTEST_FILE=${PROJECT_SOURCE_DIR}/tests/books/empty
	-DNAME=nobook_arg
	-P ${PROJECT_SOURCE_DIR}/cmake/expect.cmake)


add_test(NAME "fenbook_argument"
	COMMAND ${CMAKE_COMMAND}
	-DTEST_PROG=$<TARGET_FILE:taltos>
	-DTEST_PROG_ARGS1=--fenbook
	-DTEST_PROG_ARGS2=${PROJECT_SOURCE_DIR}/tests/books/fen_dummy.txt
	-DCMP_PROG=$<TARGET_FILE:cmp_text>
	-DTEST_FILE=${PROJECT_SOURCE_DIR}/tests/books/fen_dummy
	-DNAME=fenbook_arg
	-P ${PROJECT_SOURCE_DIR}/cmake/expect.cmake)

add_test(NAME "polyglotbook_argument"
	COMMAND ${CMAKE_COMMAND}
	-DTEST_PROG=$<TARGET_FILE:taltos>
	-DTEST_PROG_ARGS1=--book
	-DTEST_PROG_ARGS2=dummy_book_4.bin
	-DCMP_PROG=$<TARGET_FILE:cmp_text>
	-DTEST_FILE=${PROJECT_SOURCE_DIR}/tests/books/polyg_dummy
	-DNAME=polyglot_book_arg
	-P ${PROJECT_SOURCE_DIR}/cmake/expect.cmake)

add_executable(test_fen_book fen_book.c)
target_link_libraries(test_fen_book tests_main taltos_code)
add_test(NAME fen_book COMMAND $<TARGET_FILE:test_fen_book> ${PROJECT_SOURCE_DIR}/tests/books/fen_dummy.txt)

add_executable(test_polyglot_book polyglot_book.c)
target_link_libraries(test_polyglot_book tests_main taltos_code)
add_test(NAME polyglot_book COMMAND $<TARGET_FILE:test_polyglot_book> dummy_book_4.bin)
