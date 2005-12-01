noinst_PROGRAMS = lambda
BUILT_SOURCES = grammar.tab.cpp lexer.yy.cpp
lambda_SOURCES = lambda.cpp Reducer.cpp Functions.cpp grammar.tab.cpp lexer.yy.cpp
lambda_LDFLAGS = -static
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = Reducer.h grammar.tab.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = ../libgridxslt.la @ALL_LIBS@
CLEANFILES = *.da *.bb *.bbg *.gcov grammar.tab.cpp lexer.yy.cpp grammar.tab.h

grammar.tab.cpp: grammar.y
	bison -t -p ml -d grammar.y
	mv -f grammar.tab.c grammar.tab.cpp

grammar.tab.h: grammar.y
	bison -t -p ml -d grammar.y
	mv -f grammar.tab.c grammar.tab.cpp

lexer.yy.cpp: lexer.l
	flex -P ml lexer.l
	mv -f lexer.yy.c lexer.yy.cpp
