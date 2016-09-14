prog=$1
test=$2
$prog --notest < $test.in > .out
if [ $? -ne 0 ]
then
	exit 1
exit
fi
cmp .out $test.out
