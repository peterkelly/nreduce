LDADD = ../libnreduce.la libsrc.la @ALL_LIBS@
noinst_PROGRAMS = nreduce
noinst_HEADERS = assembler.h nreduce.h
nreduce_SOURCES = nreduce.c
nreduce_LDFLAGS = -static

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = assembler.c

CLEANFILES = *.da *.bb *.bbg *.gcov
