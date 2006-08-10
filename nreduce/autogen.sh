#!/bin/bash
libtoolize --force --copy
aclocal -I .
autoconf
autoheader
automake --add-missing --force-missing -c
