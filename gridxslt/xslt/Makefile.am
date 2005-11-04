noinst_LTLIBRARIES = libxslt.la
noinst_PROGRAMS = testxslt testxpath
BUILT_SOURCES = grammar.tab.cpp lexer.yy.cpp
CLEANFILES = grammar.tab.cpp lexer.yy.cpp grammar.tab.h
libxslt_la_SOURCES = grammar.tab.cpp lexer.yy.cpp Statement.cpp Expression.cpp Parse.cpp Output.cpp Compilation.cpp Builtins.cpp Special.cpp String.cpp Sequence.cpp Node.cpp Boolean.cpp Numeric.cpp Accessor.cpp Context.cpp testxpath_impl.cpp testxslt_impl.cpp TypeConversion.cpp
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = grammar.tab.h Statement.h Expression.h Parse.h Output.h Compilation.h Builtins.h Special.h
testxslt_SOURCES = testxslt.cpp
testxpath_SOURCES = testxpath.cpp
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libxslt.la ../dataflow/libdataflow.la ../xmlschema/libxmlschema.la ../util/libutil.la @ALL_LIBS@

grammar.tab.cpp: grammar.y
	bison -d grammar.y
	mv -f grammar.tab.c grammar.tab.cpp

grammar.tab.h: grammar.y
	bison -o grammar.tab.cpp -d grammar.y

lexer.yy.cpp: lexer.l
	flex lexer.l
	mv -f lexer.yy.c lexer.yy.cpp
