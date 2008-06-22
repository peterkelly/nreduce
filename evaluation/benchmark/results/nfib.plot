set terminal postscript eps color
set out "/Users/peter/evaluation/test2/plots/nfib.eps"
set key left
set xlabel "n"
set ylabel "User time (s)"
set title "nfib"
set size 0.75,0.75
plot \
  "/Users/peter/evaluation/test2/plots/nfib.dat" using 1:2 with lines linewidth 2 title "c", \
  "/Users/peter/evaluation/test2/plots/nfib.dat" using 1:3 with lines linewidth 2 title "elc", \
  "/Users/peter/evaluation/test2/plots/nfib.dat" using 1:4 with lines linewidth 2 title "java", \
  "/Users/peter/evaluation/test2/plots/nfib.dat" using 1:5 with lines linewidth 2 title "js", \
  "/Users/peter/evaluation/test2/plots/nfib.dat" using 1:6 with lines linewidth 2 title "perl", \
  "/Users/peter/evaluation/test2/plots/nfib.dat" using 1:7 with lines linewidth 2 title "python"
