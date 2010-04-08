#!/bin/bash

if (($# < 3)); then
  echo "Usage: $0 <name> <smalllabel> <biglabel>"
  exit 1
fi

NAME=$1
SMALL_LABEL=$2
BIG_LABEL=$3

t1of=`grep '^1 ' $NAME-small-o.dat | awk '{print $2}'`
t1yf=`grep '^1 ' $NAME-small-y.dat | awk '{print $2}'`

t1os=`grep '^1 ' $NAME-big-o.dat | awk '{print $2}'`
t1ys=`grep '^1 ' $NAME-big-y.dat | awk '{print $2}'`

rm -f $NAME-speedup.plot

if [ -e amdahl_p ]; then
  cat amdahl_p >> $NAME-speedup.plot
  echo 'amdahl(n) = 1/((1-P)+P/n)' >> $NAME-speedup.plot
fi

cat >>$NAME-speedup.plot <<HERE
set terminal postscript eps color size 15cm, 9cm
set output "$NAME-speedup.eps"
set size 0.75, 0.75

set xlabel "# nodes"
set ylabel "Speedup"
set xtics 4
set ytics 4
set format y "%4.f"

set style data lines
set key top left
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "blue" linetype 3
set style line 4 linewidth 2 linecolor rgbcolor "#00CC00" linetype 3
set style line 5 linewidth 2 linecolor rgbcolor "magenta" linetype 1
set style fill solid 0.2

plot x title "Ideal speedup" with lines ls 0, \\
  amdahl(x) title "Amdahl prediction" with lines ls 5, \\
  "$NAME-small-o.dat" using 1:($t1of/\$2) title "Orchestration ($SMALL_LABEL)" with lines ls 1, \\
  "$NAME-small-y.dat" using 1:($t1yf/\$2) title "Choreography ($SMALL_LABEL)" with lines ls 2, \\
  "$NAME-big-o.dat" using 1:($t1os/\$2) title "Orchestration ($BIG_LABEL)" with lines ls 3, \\
  "$NAME-big-y.dat" using 1:($t1ys/\$2) title "Choreography ($BIG_LABEL)" with lines ls 4
HERE

if [ ! -e amdahl_p ]; then
  grep -v amdahl $NAME-speedup.plot > temp
  mv -f temp $NAME-speedup.plot
fi

gnuplot $NAME-speedup.plot
echo Generated $NAME-speedup.eps
