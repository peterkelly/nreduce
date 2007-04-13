all: shellserver echoserver dataserver webserver readserver genstrand slowget

shellserver: shellserver.c util.h util.c
	gcc -Wall -O0 -ggdb3 -o shellserver shellserver.c util.c

echoserver: echoserver.c util.h util.c
	gcc -Wall -O0 -ggdb3 -o echoserver echoserver.c util.c

dataserver: dataserver.c util.h util.c
	gcc -Wall -O0 -ggdb3 -o dataserver dataserver.c util.c

webserver: webserver.c util.h util.c
	gcc -Wall -O0 -ggdb3 -lxml2 -I/usr/include/libxml2 -o webserver webserver.c util.c

readserver: readserver.c util.h util.c
	gcc -Wall -O0 -ggdb3 -lxml2 -I/usr/include/libxml2 -o readserver readserver.c util.c

genstrand: genstrand.c
	gcc -Wall -O0 -ggdb3 -o genstrand genstrand.c

slowget: slowget.c
	gcc -Wall -O0 -ggdb3 -o slowget slowget.c

clean:
	rm -f shellserver echoserver dataserver webserver readserver genstrand slowget
