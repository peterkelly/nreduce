noinst_LTLIBRARIES = libmodules.la
noinst_PROGRAMS = 
noinst_HEADERS = 
BUILT_SOURCES = util.c http.c
EXTRA_DIST = util.elc http.elc
libmodules_la_SOURCES = util.c http.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov util.c http.c

util.c: util.elc
	echo 'const char *util_module = ' > util.c
	cat util.elc | sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> util.c
	echo ';' >> util.c

http.c: http.elc
	echo 'const char *http_module = ' > http.c
	cat http.elc | sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> http.c
	echo ';' >> http.c
