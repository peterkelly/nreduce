noinst_PROGRAMS = process
noinst_HEADERS = ds.h
BUILT_SOURCES = grammar.tab.c lexer.yy.c
EXTRA_DIST = grammar.y lexer.l
process_SOURCES = process.c latex.c support.c grammar.tab.c lexer.yy.c
process_LDADD = ../cxslt/libcxslt.la ../network/libnetwork.la @ALL_LIBS@
INCLUDES = -I@top_srcdir@ -I/usr/include/libxml2
CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.c lexer.yy.c grammar.tab.h

grammar.tab.c: grammar.y
	bison -p ds -d grammar.y

lexer.yy.c: lexer.l
	flex -Pds lexer.l
