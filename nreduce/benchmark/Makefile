objects = nsieve nsieve.class matmulti matmulti.class matmultd matmultd.class bintree bintree.class \
	quicksort quicksort.class quicksort_inplace.class quicksort_inplace mergesort mergesort.class \
	mergesort_inplace mergesort_inplace.class nfib nfib.class mandelbrot mandelbrot.class

all: $(objects)

nsieve: nsieve.c
	gcc -Wall -O3 -o nsieve nsieve.c

matmulti: matmulti.c
	gcc -Wall -O3 -o matmulti matmulti.c

matmultd: matmultd.c
	gcc -Wall -O3 -o matmultd matmultd.c

bintree: bintree.c
	gcc -Wall -O3 -o bintree bintree.c

quicksort: quicksort.c
	gcc -Wall -O3 -o quicksort quicksort.c

quicksort_inplace: quicksort_inplace.c
	gcc -Wall -O3 -o quicksort_inplace quicksort_inplace.c

mergesort: mergesort.c
	gcc -Wall -O3 -o mergesort mergesort.c

mergesort_inplace: mergesort_inplace.c
	gcc -Wall -O3 -o mergesort_inplace mergesort_inplace.c

nfib: nfib.c
	gcc -Wall -O3 -o nfib nfib.c

mandelbrot: mandelbrot.c
	gcc -Wall -O3 -o mandelbrot mandelbrot.c

%.class: %.java
	javac $<

clean:
	rm -f $(objects)
