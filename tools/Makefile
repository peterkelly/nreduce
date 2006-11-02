all: parsh showload nobuftail

parsh: parsh.c
	gcc -g -Wall -o parsh parsh.c

showload: showload.c
	gcc -g -Wall -o showload showload.c -lcurses

nobuftail: nobuftail.c
	gcc -g -Wall -o nobuftail nobuftail.c

clean:
	rm -f parsh showload nobuftail
