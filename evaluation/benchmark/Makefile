objects = nsieve nsieve.class matmult2 matmult2.class matmult matmult.class bintree bintree.class \
	quicksort quicksort.class quicksort2.class quicksort2 mergesort mergesort.class \
	mergesort2 mergesort2.class nfib nfib.class mandelbrot mandelbrot.class ProcessResults.class \
	GenScripts.class hsbintree hsmandelbrot hsnfib hsnfibdouble hsmatmult hsmergesort hsquicksort \
	hsnsieve

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

hsbintree: hsbintree.hs
	ghc -O3 -o hsbintree hsbintree.hs

hsmandelbrot: hsmandelbrot.hs
	ghc -O3 -o hsmandelbrot hsmandelbrot.hs

hsnfib: hsnfib.hs
	ghc -O3 -o hsnfib hsnfib.hs

hsnfibdouble: hsnfibdouble.hs
	ghc -O3 -o hsnfibdouble hsnfibdouble.hs

hsmatmult: hsmatmult.hs
	ghc -O3 -o hsmatmult hsmatmult.hs

hsmergesort: hsmergesort.hs
	ghc -O3 -o hsmergesort hsmergesort.hs

hsquicksort: hsquicksort.hs
	ghc -O3 -o hsquicksort hsquicksort.hs

hsnsieve: hsnsieve.hs
	ghc -O3 -o hsnsieve hsnsieve.hs

%.class: %.java
	javac $<

clean:
	rm -f $(objects)
	rm -f *.class
	rm -f *.hi
