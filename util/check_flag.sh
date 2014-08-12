#!/bin/sh
flag=$1
${CC} $flag tmp.c > /dev/null 2>&1
status=$?
if test $status = 0 ; then
    printf " %s " $flag
fi
rm -f a.out
exit $status
