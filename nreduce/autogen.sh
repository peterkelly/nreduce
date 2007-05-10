#!/bin/bash
if [ -e /usr/bin/glibtoolize ]; then
  glibtoolize --force --copy
else
  libtoolize --force --copy
fi
aclocal -I .
autoconf
autoheader
automake --add-missing --force-missing -c
