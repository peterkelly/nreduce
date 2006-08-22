LDADD = @ALL_LIBS@
noinst_PROGRAMS = nreduce
noinst_HEADERS = assembler.h gcode.h nreduce.h
BUILT_SOURCES = grammar.tab.c lexer.yy.c
EXTRA_DIST = grammar.y lexer.l grammar.tab.h
nreduce_SOURCES = assembler.c \
	debug.c \
	gmachine.c \
	reduction.c \
	builtins.c \
	gcode.c \
	memory.c \
	strictness.c \
	util.c \
	nreduce.c \
	super.c \
	lifting.c \
	graph.c \
	renaming.c \
	resolve.c \
	reorder.c \
	source.c \
	grammar.tab.c \
	lexer.yy.c
#	life.c
#	testasm.c

CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.c lexer.yy.c grammar.tab.h

grammar.tab.c: grammar.y
	bison -d grammar.y

lexer.yy.c: lexer.l
	flex lexer.l
