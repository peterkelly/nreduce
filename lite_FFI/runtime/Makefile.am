noinst_LTLIBRARIES = libruntime.la
noinst_PROGRAMS = 
noinst_HEADERS = runtime.h
libruntime_la_SOURCES = task.c \
	builtins.c \
	memory.c \
	reduction.c \
	rngs.c rngs.h \
	rvgs.c rvgs.h \
	zzip.h \
	extfuncs.c extfuncs.h

libruntime_la_LDFLAGS = -lzzip

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov
