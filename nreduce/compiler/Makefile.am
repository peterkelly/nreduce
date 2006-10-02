noinst_LTLIBRARIES = libcompiler.la
noinst_PROGRAMS = 
noinst_HEADERS = util.h source.h bytecode.h
BUILT_SOURCES = grammar.tab.c lexer.yy.c
EXTRA_DIST = grammar.y lexer.l grammar.tab.h
libcompiler_la_SOURCES = util.c \
	strictness.c \
	super.c \
	lifting.c \
	graph.c \
	renaming.c \
	resolve.c \
	reorder.c \
	source.c \
	bytecode.c \
	debug.c \
	grammar.tab.c \
	lexer.yy.c
INCLUDES = -I@top_srcdir@
CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.c lexer.yy.c grammar.tab.h

grammar.tab.c: grammar.y
	bison -d grammar.y

lexer.yy.c: lexer.l
	flex lexer.l

