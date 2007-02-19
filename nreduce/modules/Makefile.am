noinst_LTLIBRARIES = libmodules.la
noinst_PROGRAMS = 
noinst_HEADERS = 
libmodules_la_SOURCES = util_module.c http_module.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov util_module.c http_module.c

util_module.c: util.l
	echo 'const char *util_module = ' > util_module.c
	cat util.l | sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> util_module.c
	echo ';' >> util_module.c

http_module.c: http.l
	echo 'const char *http_module = ' > http_module.c
	cat http.l | sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> http_module.c
	echo ';' >> http_module.c
