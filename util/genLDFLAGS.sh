#!/bin/sh
UTIL_DIR=`dirname $0`
case $1 in
    gprof)
    printf " -pg -fprofile-generate "
    ;;
    gprofuse)
    printf " -fprofile-use "
    ;;
esac
if test "x$2" != "x" ; then
	echo $LDFLAGS
	exit 0
fi
printf "#include <pthread.h>\nint main() {" > tmp.c
printf "pthread_testcancel();return 0;}" >> tmp.c
export CC=${CC:=cc}
$UTIL_DIR/check_flag.sh -lpthread || \
    $UTIL_DIR/check_flag.sh -pthread || \
    $UTIL_DIR/check_flag.sh -pthreads || \
rm -f tmp.c
