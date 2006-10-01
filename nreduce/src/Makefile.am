LDADD = ../libnreduce.la libsrc.la @ALL_LIBS@
noinst_PROGRAMS = nreduce
noinst_HEADERS = assembler.h nreduce.h
BUILT_SOURCES = prelude.c
EXTRA_DIST = prelude.l
nreduce_SOURCES = nreduce.c
nreduce_LDFLAGS = -static

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = assembler.c \
	prelude.c

CLEANFILES = *.da *.bb *.bbg *.gcov prelude.c

prelude.c: prelude.l
	echo 'const char *prelude = ' > prelude.c
	cat prelude.l | sed -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> prelude.c
	echo ';' >> prelude.c
