objects = nsieve nsieve.class matmult2 matmult2.class matmult matmult.class bintree bintree.class \
	quicksort quicksort.class quicksort2.class quicksort2 mergesort mergesort.class \
	mergesort2 mergesort2.class nfib nfib.class mandelbrot mandelbrot.class ProcessResults.class \
	GenScripts.class

all: $(objects)

nsieve: nsieve.c
	gcc -Wall -O3 -o nsieve nsieve.c

matmult2: matmult2.c
	gcc -Wall -O3 -o matmult2 matmult2.c

matmult: matmult.c
	gcc -Wall -O3 -o matmult matmult.c -lm

bintree: bintree.c
	gcc -Wall -O3 -o bintree bintree.c

quicksort: quicksort.c
	gcc -Wall -O3 -o quicksort quicksort.c

quicksort2: quicksort2.c
	gcc -Wall -O3 -o quicksort2 quicksort2.c

mergesort: mergesort.c
	gcc -Wall -O3 -o mergesort mergesort.c

mergesort2: mergesort2.c
	gcc -Wall -O3 -o mergesort2 mergesort2.c

nfib: nfib.c
	gcc -Wall -O3 -o nfib nfib.c

mandelbrot: mandelbrot.c
	gcc -Wall -O3 -o mandelbrot mandelbrot.c -lm

%.class: %.java
	javac $<

clean:
	rm -f $(objects)
	rm -f *.class
