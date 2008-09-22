all: data requests Requests.class manythreads ManyThreads.class

data: data.c
	gcc -Wall -o data data.c

requests: requests.c
	gcc -Wall -o requests requests.c -lpthread

Requests.class: Requests.java
	javac Requests.java

manythreads: manythreads.c
	gcc -Wall -o manythreads manythreads.c -lpthread

ManyThreads.class: ManyThreads.java
	javac ManyThreads.java

clean:
	rm -f data requests *.class manythreads
