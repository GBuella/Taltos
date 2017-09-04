# vim: set filetype=cmake :
# vim: set noet ts=8 sw=8 cinoptions=+4,(4:
#
# Run a program with input piped into, and compare the output
# with the expected output.

execute_process(COMMAND ${TEST_PROG}
	INPUT_FILE ${TEST_FILE}.in
	OUTPUT_FILE ${NAME}.out
	RESULT_VARIABLE HAD_ERROR)

if(HAD_ERROR)
	message(FATAL_ERROR "Test failed")
endif()

execute_process(COMMAND ${CMP_PROG} ${NAME}.out ${TEST_FILE}.out
	RESULT_VARIABLE DIFFERENT)

if(DIFFERENT)
	message(FATAL_ERROR "Test failed - files differ")
endif()
