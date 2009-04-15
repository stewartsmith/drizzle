#!/bin/sh

failed=0

if test -z $BINDIR ; then
  BINDIR=.
fi

check_encode () {
    bytes=`${BINDIR}/length --hex --encode $1`
    shift
    expected="$@"
    if test "$bytes" != "$expected"; then
        failed=`expr $failed + 1`
        echo "Got '$bytes' but expected '$expected'"
    fi
}

check_invertible () {
    bytes=`${BINDIR}/length --encode $value`
    result=`${BINDIR}/length --decode $bytes`
    if test $result -ne $value; then
        failed=`expr $failed + 1`
        echo "Got $result but expected $value"
    fi
}

# check that the encoding is correct
check_encode 2     0x2
check_encode 255   0xff
check_encode 256   0x0 0x0 0x1
check_encode 65535 0x0 0xff 0xff

# Check that we can decode what we have encoded
for value in 2 255 256 65535 65536 \
    `expr 16 "*" 65536` \
    `expr 16 "*" 65536 + 255`
do
    check_invertible $value
done

exit $failed
