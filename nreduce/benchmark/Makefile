all: nfib nfib.class

%.c: %.l

nfib: nfib.c
	gcc -o nfib nfib.c

nfib.class:
	javac nfib.java

clean:
	rm -f nfib nfib.class
