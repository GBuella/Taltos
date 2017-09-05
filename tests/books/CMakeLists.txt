
add_test(NAME "nobook_argument"
	COMMAND ${CMAKE_COMMAND}
	-DTEST_PROG=$<TARGET_FILE:taltos>
	-DTEST_PROG_ARGS1=--nobook
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
