LDADD = @ALL_LIBS@
noinst_PROGRAMS = nreduce
noinst_HEADERS = assembler.h gcode.h nreduce.h
BUILT_SOURCES = grammar.tab.c lexer.yy.c prelude.c
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
	data.c \
	console.c \
	process.c \
	grammar.tab.c \
	lexer.yy.c \
	prelude.c
#	life.c
#	testasm.c

CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.c lexer.yy.c grammar.tab.h prelude.c

grammar.tab.c: grammar.y
	bison -d grammar.y

lexer.yy.c: lexer.l
	flex lexer.l

prelude.c: prelude.l
	echo 'const char *prelude = ' > prelude.c
	cat prelude.l | sed -e 's/\"/\\"/g' -e 's/^/\"/' -e 's/$$/\\n\"/' >> prelude.c
	echo ';' >> prelude.c
