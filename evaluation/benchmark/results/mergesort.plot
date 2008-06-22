set terminal postscript eps color
set out "/Users/peter/evaluation/test2/plots/mergesort.eps"
set key left
set xlabel "n"
set ylabel "User time (s)"
set title "mergesort"
set size 0.75,0.75
plot \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:2 with lines linewidth 2 title "c", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:3 with lines linewidth 2 title "c2", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:4 with lines linewidth 2 title "elc", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:5 with lines linewidth 2 title "java", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:6 with lines linewidth 2 title "java2", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:7 with lines linewidth 2 title "js", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:8 with lines linewidth 2 title "js2", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:9 with lines linewidth 2 title "perl", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:10 with lines linewidth 2 title "perl2", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:11 with lines linewidth 2 title "python", \
  "/Users/peter/evaluation/test2/plots/mergesort.dat" using 1:12 with lines linewidth 2 title "python2"
