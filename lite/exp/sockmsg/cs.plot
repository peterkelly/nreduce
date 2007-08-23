set terminal postscript eps
set out "cs.eps"

set xlabel "I/O buffer size (bytes)"
set ylabel "Context switches (1000s)"

set xtics 8192
set ytics 100
set title "Context switches vs. block size"

set size 0.5,0.5

plot \
  "results1" using 1:($5/1000) with lines linewidth 2 title "Socket APIs", \
  "results2" using 1:($5/1000) with lines linewidth 2 title "Messaging layer"
