PROGRAMS = parsh showload loadbal waitconn nobuftail runtests measure svc_compute svc_compdata compute connections average add min max pow2 proxy transfer

almostall: $(PROGRAMS)

all: almostall saxonclient

parsh: parsh.c
	gcc -g -Wall -o parsh parsh.c

showload: showload.c
	gcc -g -Wall -o showload showload.c -lncurses

loadbal: loadbal.c portset.h portset.c
	gcc -g -Wall -o loadbal loadbal.c portset.c

waitconn: waitconn.c
	gcc -g -Wall -o waitconn waitconn.c

nobuftail: nobuftail.c
	gcc -g -Wall -o nobuftail nobuftail.c

runtests: runtests.c
	gcc -g -Wall -o runtests runtests.c

measure: measure.c
	gcc -g -Wall -o measure measure.c -lpthread

svc_compute: svc_compute.c
	gcc -g -m32 -Wall -o svc_compute svc_compute.c -lpthread

svc_compdata: svc_compdata.c
	gcc -g -m32 -Wall -o svc_compdata svc_compdata.c -lpthread

compute: compute.c
	gcc -g -Wall -o compute compute.c -lpthread

connections: connections.c portset.h portset.c
	gcc -g -Wall -o connections connections.c portset.c -lpthread

average: average.c
	gcc -g -Wall -o average average.c

add: add.c
	gcc -g -Wall -o add add.c

min: min.c
	gcc -g -Wall -o min min.c

max: max.c
	gcc -g -Wall -o max max.c

pow2: pow2.c
	gcc -g -Wall -o pow2 pow2.c -lm

proxy: proxy.c
	gcc -g -Wall -o proxy proxy.c -lpthread

transfer: transfer.c
	gcc -g -Wall -o transfer transfer.c -lpthread

saxonclient: saxonclient.c
	gcc -g -Wall -o saxonclient saxonclient.c -lcurl

saxonwebapp: SaxonServlet.java
	javac SaxonServlet.java
	cp SaxonServlet.class saxon/WEB-INF/classes

clean:
	rm -f $(PROGRAMS) saxonclient SaxonServlet.class saxon/WEB-INF/classes/SaxonServlet.class
	rm -rf *.dSYM
