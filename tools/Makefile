all: parsh showload

parsh: parsh.c
	gcc -g -Wall -o parsh parsh.c

showload: showload.c
	gcc -g -Wall -o showload showload.c -lcurses

clean:
	rm -f parsh showload
