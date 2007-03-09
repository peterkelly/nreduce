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

clean:
	rm -f parsh showload nobuftail runtests saxonclient
