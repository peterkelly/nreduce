#!/bin/bash
if [ -f Makefile ]; then
  make distclean
fi
rm -rf autom4te.cache
rm -f config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing aclocal.m4
rm -f */Makefile.in */*/Makefile.in
