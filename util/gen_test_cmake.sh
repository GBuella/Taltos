printf "# file generated by util/gen_test_cmake.sh\n"
printf "# do not edit manually\n"
echo
testdir=$1
prefix=$2
for f in $testdir/*.in
do
	test=${f%.in}
	printf "add_test(\"%s%s\"\n" $prefix $(basename $test)
	printf "\t\${PROJECT_SOURCE_DIR}/util/runtest.sh\n"
	printf "\t\${PROJECT_BINARY_DIR}/taltos\n"
	printf "\t\${PROJECT_SOURCE_DIR}/tests/%s )\n" $test
done
