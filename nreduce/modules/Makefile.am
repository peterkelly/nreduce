noinst_LTLIBRARIES = libmodules.la
noinst_PROGRAMS = 
noinst_HEADERS = 
BUILT_SOURCES = util_module.c http_module.c
EXTRA_DIST = util.elc http.elc
libmodules_la_SOURCES = util_module.c http_module.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov util_module.c http_module.c

util_module.c: util.elc
	echo 'const char *util_module = ' > util_module.c
	cat util.elc | sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> util_module.c
	echo ';' >> util_module.c

http_module.c: http.elc
	echo 'const char *http_module = ' > http_module.c
	cat http.elc | sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> http_module.c
	echo ';' >> http_module.c
