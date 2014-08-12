#!/bin/sh
UTIL_DIR=`dirname $0`
export CC=${CC:=cc}
echo "int main() {return 0;}" > tmp.c
case $1 in
    prod)
        $UTIL_DIR/check_flag.sh -DNDEBUG
        $UTIL_DIR/check_flag.sh -O3
        ;;
    gprof)
        $UTIL_DIR/check_flag.sh -DNDEBUG
        $UTIL_DIR/check_flag.sh -O3
        printf " -pg -fprofile-generate "
        ;;
    gprofuse)
        $UTIL_DIR/check_flag.sh -DNDEBUG
        $UTIL_DIR/check_flag.sh -O3
        printf " -fprofile-use "
        ;;
    *)
        $UTIL_DIR/check_flag.sh -g
        $UTIL_DIR/check_flag.sh -O0
esac
if test "x$2" != "x" ; then
    echo $CFLAGS ' -DSLIDING_BYTE_LOOKUP -DUSE_KNIGHT_LOOKUP_TABLE'
    rm -f tmp.c
    exit 0
fi
$UTIL_DIR/check_flag.sh -std=c99
$UTIL_DIR/check_flag.sh -Wall
$UTIL_DIR/check_flag.sh -Wextra
$UTIL_DIR/check_flag.sh -pedantic
$UTIL_DIR/check_flag.sh -Werror
$UTIL_DIR/check_flag.sh -march=native
echo "#include <x86intrin.h>" > tmp.c
echo "int main() {return __builtin_ctzll(42323ULL);}" > tmp.c
$UTIL_DIR/check_flag.sh -mpopcnt
echo '-DSLIDING_BYTE_LOOKUP -DUSE_KNIGHT_LOOKUP_TABLE'
rm -f tmp.c
