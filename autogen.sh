#! /bin/sh

libtoolize --copy --force
aclocal -I m4 
autoheader
autoconf
automake -a -c
