SUBDIRS = compiler runtime . src
AUTOMAKE_OPTIONS = foreign
lib_LTLIBRARIES = libnreduce.la
libnreduce_la_SOURCES =
libnreduce_la_LIBADD = compiler/libcompiler.la runtime/libruntime.la
CLEANFILES = *.da *.bb *.bbg *.gcov
EXTRA_DIST = samples/*.l tests/*.test
