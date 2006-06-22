#!/bin/bash
libtoolize --force --copy
aclocal -I .
autoconf
automake --add-missing --force-missing -c
