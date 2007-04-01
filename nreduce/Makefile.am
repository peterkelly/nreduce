SUBDIRS = modules compiler runtime . src
AUTOMAKE_OPTIONS = foreign
lib_LTLIBRARIES = libnreduce.la
libnreduce_la_SOURCES =
libnreduce_la_LIBADD = modules/libmodules.la compiler/libcompiler.la runtime/libruntime.la
CLEANFILES = *.da *.bb *.bbg *.gcov
EXTRA_DIST = samples/*.elc elc-mode.el
