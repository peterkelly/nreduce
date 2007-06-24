set terminal postscript eps color
set out "time.eps"

set xlabel "Message count"
set ylabel "Time (s)"

#set xtics 8192
#set ytics 5
set title "Time to transmit n messages"

#set size 0.5,0.5

plot \
  "results1b" using 1:2 with lines linewidth 2 title "1 byte", \
  "results1k" using 1:2 with lines linewidth 2 title "1 kb", \
  "results10k" using 1:2 with lines linewidth 2 title "10 kb", \
  "results100k" using 1:2 with lines linewidth 2 title "100 kb"
