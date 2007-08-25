noinst_LTLIBRARIES = libruntime.la
noinst_PROGRAMS = 
noinst_HEADERS = runtime.h chord.h assembler.h messages.h
libruntime_la_SOURCES = worker.c \
	task.c \
	builtins.c \
	memory.c \
	data.c \
	interpreter.c \
	reduction.c \
	manager.c \
	client.c \
	graph.c \
	tosyntax.c \
	trace.c \
	chord.c \
	testchord.c \
	assembler.c \
	native.c \
	java.c

INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov
