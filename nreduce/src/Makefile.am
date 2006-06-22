LDADD = @ALL_LIBS@
noinst_PROGRAMS = nreduce
noinst_HEADERS = assembler.h gcode.h nreduce.h
BUILT_SOURCES = grammar.tab.c lexer.yy.c
nreduce_SOURCES = assembler.c \
	extra.c \
	gmachine.c \
	reduction.c \
	builtins.c \
	gcode.c \
	jit.c \
	memory.c \
	strictness.c \
	util.c \
	code.c \
	gcodeold.c \
	nreduce.c \
	super.c \
	grammar.tab.c \
	lexer.yy.c
#	life.c
#	lifel.c
#	testasm.c

CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.c lexer.yy.c grammar.tab.h

grammar.tab.c: grammar.y
	bison -d grammar.y

grammar.tab.h: grammar.y
	bison -d grammar.y

lexer.yy.c: lexer.l
	flex lexer.l

