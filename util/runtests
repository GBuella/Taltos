prog=$1
shift
while [ "$1" != "" ]
do
    test=${1%.in}
    printf "%s " $test
    $prog -t < $test.in > .out 2> .time
    if [ $? -ne 0 ]
    then
        echo Failed
        exit 1
    exit
    fi
    t=`tail -1 .time`
    if $(diff .out $test.out > /dev/null)
    then
        echo OK $t s
    else
        echo Failed
        exit 1
    fi
    shift
done
