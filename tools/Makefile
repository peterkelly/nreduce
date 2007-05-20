all: parsh showload nobuftail runtests saxonclient

parsh: parsh.c
	gcc -g -Wall -o parsh parsh.c

showload: showload.c
	gcc -g -Wall -o showload showload.c -lcurses

nobuftail: nobuftail.c
	gcc -g -Wall -o nobuftail nobuftail.c

runtests: runtests.c
	gcc -g -Wall -o runtests runtests.c

saxonclient: saxonclient.c
	gcc -g -Wall -o saxonclient saxonclient.c -lcurl

saxonwebapp: SaxonServlet.java
	javac SaxonServlet.java
	cp SaxonServlet.class saxon/WEB-INF/classes

clean:
	rm -f parsh showload nobuftail runtests saxonclient SaxonServlet.class saxon/WEB-INF/classes/SaxonServlet.class
