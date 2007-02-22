noinst_LTLIBRARIES = libmodules.la
noinst_PROGRAMS = 
noinst_HEADERS = modules.h
EXTRA_DIST = *.elc convert_modules
BUILT_SOURCES = modules.c
libmodules_la_SOURCES = modules.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov util.c http.c prelude.c

modules.c: *.elc convert_modules
	./convert_modules
