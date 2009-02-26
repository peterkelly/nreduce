#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <plotname> <title>"
  exit 1
fi

PLOT_NAME=$1
TITLE=$2

t1o=`grep '^1 ' $PLOT_NAME-o.dat | awk '{print $2}'`
t1y=`grep '^1 ' $PLOT_NAME-y.dat | awk '{print $2}'`

rm -f $PLOT_NAME-speedup.plot

if [ -e amdahl_p ]; then
  cat amdahl_p >> $PLOT_NAME-speedup.plot
  echo 'amdahl(n) = 1/((1-P)+P/n)' >> $PLOT_NAME-speedup.plot
fi

cat >>$PLOT_NAME-speedup.plot <<HERE
set terminal postscript eps color size 15cm, 10cm
set output "$PLOT_NAME-speedup.eps"
set size 0.75, 0.75

set xlabel "# nodes"
set ylabel "Speedup"
set xtics 4
set ytics 4
set format y "%4.f"
set title "$TITLE - speedup"

set style data lines
set key top left
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "magenta" linetype 1
set style fill solid 0.2

plot x title "Ideal speedup" with lines ls 0, \\
     amdahl(x) title "Amdahl prediction" with lines ls 3, \\
     "$PLOT_NAME-o.dat" using 1:($t1o/\$2) title "Orchestration" with lines ls 1, \\
     "$PLOT_NAME-y.dat" using 1:($t1y/\$2) title "Choreography" with lines ls 2
HERE

if [ ! -e amdahl_p ]; then
  grep -v amdahl $PLOT_NAME-speedup.plot > temp
  mv -f temp $PLOT_NAME-speedup.plot
fi

gnuplot $PLOT_NAME-speedup.plot
echo Generated $PLOT_NAME-speedup.eps
