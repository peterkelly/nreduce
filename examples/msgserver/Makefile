all: msgserver msgclient

msgserver: msgserver.c
	gcc -g -Wall -o msgserver msgserver.c

msgclient: msgclient.c
	gcc -g -Wall -o msgclient msgclient.c

clean:
	rm -f msgserver msgclient
