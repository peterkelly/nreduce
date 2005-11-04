noinst_LTLIBRARIES = libxslt.la
noinst_PROGRAMS = testxslt testxpath
BUILT_SOURCES = grammar.tab.c lexer.yy.c
CLEANFILES = grammar.tab.c lexer.yy.c grammar.tab.h
libxslt_la_SOURCES = grammar.tab.c lexer.yy.c Statement.cpp Expression.cpp parse.c output.c compile.c builtins.c special.c string.c sequence.c node.c boolean.c numeric.c accessor.c context.c testxpath_impl.c testxslt_impl.c TypeConversion.cpp
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = grammar.tab.h Statement.h Expression.h parse.h output.h compile.h builtins.h special.h
testxslt_SOURCES = testxslt.c
testxpath_SOURCES = testxpath.c
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libxslt.la ../dataflow/libdataflow.la ../xmlschema/libxmlschema.la ../util/libutil.la @ALL_LIBS@

grammar.tab.c: grammar.y
	bison -d grammar.y

grammar.tab.h: grammar.y
	bison -d grammar.y

lexer.yy.c: lexer.l
	flex lexer.l
