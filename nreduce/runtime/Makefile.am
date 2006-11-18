noinst_LTLIBRARIES = libruntime.la
noinst_PROGRAMS = 
noinst_HEADERS = network.h runtime.h
libruntime_la_SOURCES = worker.c \
	network.c \
	task.c \
	builtins.c \
	memory.c \
	data.c \
	console.c \
	console2.c \
	interpreter.c \
	reduction.c \
	manager.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov
