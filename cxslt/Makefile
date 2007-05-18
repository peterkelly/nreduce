CFLAGS=-ggdb3 -Wall -O0
INCLUDES=-I/usr/include/libxml2
LIBS=-lxml2

all: cxslt

cxslt: cxslt.o wsdl.o util.o grammar.tab.o lexer.yy.o
#cxslt: cxslt.o
	gcc $(LIBS) -o cxslt cxslt.o wsdl.o util.o grammar.tab.o lexer.yy.o

cxslt.o: cxslt.c cxslt.h
	gcc $(CFLAGS) $(INCLUDES) -c cxslt.c

wsdl.o: wsdl.c cxslt.h
	gcc $(CFLAGS) $(INCLUDES) -c wsdl.c

util.o: util.c cxslt.h
	gcc $(CFLAGS) $(INCLUDES) -c util.c

grammar.tab.o: grammar.tab.c grammar.tab.h cxslt.h
	gcc $(CFLAGS) $(INCLUDES) -c grammar.tab.c

lexer.yy.o: lexer.yy.c grammar.tab.h cxslt.h
	gcc $(CFLAGS) $(INCLUDES) -c lexer.yy.c

grammar.tab.c: grammar.y
	bison -d grammar.y

lexer.yy.c: lexer.l
	flex lexer.l

clean:
	rm -f cxslt *.o grammar.tab.h grammar.tab.c lexer.yy.c

