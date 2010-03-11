#!/bin/bash

BASE=`pwd`

SCRIPT_DIR=`dirname $0`

if (($# < 2)); then
  echo "Usage: $0 <jobsdir> <plotdir>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=marks-scale
TITLE="Scalability"

mkdir -p $PLOT_DIR/plots/$PLOT_NAME

OUTPUT_FILE=$PLOT_DIR/plots/$PLOT_NAME/$PLOT_NAME

for test in marks-scale-o marks-scale-y-normal marks-scale-y-fishhalf; do
  echo "# S/call   Time       Speedup" > $PLOT_DIR/plots/$PLOT_NAME/$test.dat
  grep 'execution time' $JOBS_DIR/$test/*/output |\
  sed -e 's_^.*ms__' -e 's_/_ _' |\
  awk '{total[$1] += $5; count[$1]++}
       END { for (s in total) {
               avg=total[s]/count[s]
               printf("%-10d %-10.3f %-10.3f\n",s,avg,4096*s/2/avg) }}' |\
  sort -n -k 1 >>\
  $PLOT_DIR/plots/$PLOT_NAME/$test.dat
done

cat >$OUTPUT_FILE.plot <<HERE
set terminal postscript eps color size 15.0cm, 9cm
set output "$PLOT_NAME.eps"
set size 0.75, 0.75

set xlabel "Second per test"
set ylabel "Speedup"
set title "Speedup vs. seconds per test"
set yrange [0:32]
set ytics 4

set style data lines
set key bottom right
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "red" linetype 1
set style fill solid 0.2

plot "$PLOT_NAME-o.dat" using 1:3 title "Orchestration" with lines ls 1, \\
     "$PLOT_NAME-y-normal.dat" using 1:3 title "Choreography normal" with lines ls 2, \\
     "$PLOT_NAME-y-fishhalf.dat" using 1:3 title "Choreography fishhalf" with lines ls 3
$EXTRA
HERE

cd $BASE/$PLOT_DIR/plots/$PLOT_NAME
gnuplot $PLOT_NAME.plot
mkdir -p ../../eps
cp -f $PLOT_NAME.eps ../../eps
