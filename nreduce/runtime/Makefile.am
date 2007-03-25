noinst_LTLIBRARIES = libruntime.la
noinst_PROGRAMS = 
noinst_HEADERS = runtime.h node.h messages.h
libruntime_la_SOURCES = worker.c \
	task.c \
	builtins.c \
	memory.c \
	data.c \
	console.c \
	console2.c \
	interpreter.c \
	reduction.c \
	manager.c \
	client.c \
	graph.c \
	partial.c \
	tosyntax.c \
	trace.c \
	node.c \
	nodetest.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov
