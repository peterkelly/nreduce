#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <plotname>"
  exit 1
fi

PLOT_NAME=$1

cat >$PLOT_NAME-time.plot <<HERE
set terminal postscript eps color size 15cm, 9cm
set output "$PLOT_NAME-time.eps"
set size 0.75, 0.75

set xlabel "Source & compiled code size (Kb)"
set ylabel "Execution time (s)"
set yrange [0:]
#set xtics (1,4,8,12,16,20,24,28,32)
set xtics 64
set xrange [0:384]
set format y "%4.f"

set style data lines
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "#00CC00" linetype 3
set style fill solid 0.2

plot "$PLOT_NAME-o.dat" using 1:2 title "Orchestration" with lines ls 1, \\
     "$PLOT_NAME-y.dat" using 1:2 title "Choreography" with lines ls 2
HERE


gnuplot $PLOT_NAME-time.plot
echo Generated $PLOT_NAME-time.eps
