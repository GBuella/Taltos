
generate()
{
	printf "# file generated by tools/gen_test_cmake.sh\n"
	printf "# do not edit manually\n"
	testdir=$1
	prefix=$2
	for f in $testdir/*.in
	do
		echo
		test=${f%.in}
		printf "add_test(NAME \"%s%s\"\n" $prefix $(basename $test)
		printf "\tCOMMAND \${CMAKE_COMMAND}\n"
		printf "\t-DTEST_PROG=$<TARGET_FILE:taltos>\n"
		printf "\t-DTEST_FILE=\${PROJECT_SOURCE_DIR}/%s\n" $test
		printf "\t-P \${PROJECT_SOURCE_DIR}/cmake/expect.cmake)\n"
	done
}

generate tests/perftsuite > tests/perftsuite/CMakeLists.txt
generate tests/positions position_ > tests/positions/CMakeLists.txt