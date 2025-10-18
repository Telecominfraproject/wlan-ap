#!/bin/sh

# autogen.sh -- Autotools bootstrapping

libtoolize --copy --force
aclocal &&\
autoheader &&\
autoconf &&\
automake --add-missing --copy

