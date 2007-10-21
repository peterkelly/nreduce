SUBDIRS = modules network cxslt ds compiler runtime . src exp java
AUTOMAKE_OPTIONS = foreign
lib_LTLIBRARIES = libnreduce.la
libnreduce_la_SOURCES =
libnreduce_la_LIBADD = modules/libmodules.la network/libnetwork.la compiler/libcompiler.la runtime/libruntime.la cxslt/libcxslt.la
CLEANFILES = *.da *.bb *.bbg *.gcov
EXTRA_DIST = samples/*.elc elc-mode.el
