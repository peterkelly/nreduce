all: nfib nfib.class mandelbrot mandelbrot.class

nfib: nfib.c
	gcc -O3 -o nfib nfib.c

nfib.class:
	javac nfib.java

mandelbrot: mandelbrot.c
	gcc -O3 -o mandelbrot mandelbrot.c

mandelbrot.class: mandelbrot.java
	javac mandelbrot.java

clean:
	rm -f nfib nfib.class mandelbrot mandelbrot.class
