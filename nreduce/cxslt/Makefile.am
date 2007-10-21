noinst_LTLIBRARIES = libcxslt.la
noinst_PROGRAMS = compile
noinst_HEADERS = cxslt.h xslt.h
BUILT_SOURCES = grammar.tab.c lexer.yy.c
EXTRA_DIST = grammar.y lexer.l grammar.tab.h
libcxslt_la_SOURCES = cxslt.c tree.c analyse.c wsdl.c xmlutil.c rules.c grammar.tab.c lexer.yy.c
compile_SOURCES = compile.c
compile_LDADD = libcxslt.la ../network/libnetwork.la @ALL_LIBS@
INCLUDES = -I@top_srcdir@ -I/usr/include/libxml2
CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.c lexer.yy.c grammar.tab.h

grammar.tab.c: grammar.y
	bison -p x -d grammar.y

lexer.yy.c: lexer.l
	flex -Px lexer.l
