set terminal postscript eps
set out "time.eps"

set xlabel "I/O buffer size (bytes)"
set ylabel "Time (s)"

set xtics 8192
set ytics 5
set title "Data transfer time vs. block size"

set size 0.5,0.5

plot \
  "results1" using 1:2 with lines linewidth 2 title "Socket APIs", \
  "results2" using 1:2 with lines linewidth 2 title "Messaging layer"
