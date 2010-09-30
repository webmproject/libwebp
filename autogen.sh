#! /bin/sh -e
aclocal
automake
autoconf
./configure "$@"
