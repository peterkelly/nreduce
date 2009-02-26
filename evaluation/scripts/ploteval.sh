#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 2)); then
  echo "Usage: $0 <jobsdir> <plotdir>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=eval
TITLE="Evaluation modes"

OUTPUT_DIR=$PLOT_DIR/plots/$PLOT_NAME

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$PLOT_NAME

# Extract + collate execution times from all runs
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-lazy $PLOT_NAME-lazy $OUTPUT_DIR/$PLOT_NAME-lazy.dat
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-strict $PLOT_NAME-strict $OUTPUT_DIR/$PLOT_NAME-strict.dat

# Plot execution time and speedup
cd $OUTPUT_DIR

cat >$PLOT_NAME-time.plot <<HERE
set terminal postscript eps color size 15cm, 10cm
set output "$PLOT_NAME-time.eps"
set size 0.75, 0.75

set xlabel "# nodes"
set ylabel "Execution time (s)"
set yrange [0:]
set xtics 1
set format y "%4.f"
set title "$TITLE - execution time"

set style data lines
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "red" linetype 1

plot "$PLOT_NAME-lazy.dat" using 1:2 title "Lazy evaluation" with lines ls 1, \\
     "$PLOT_NAME-strict.dat" using 1:2 title "Strict evaluation" with lines ls 2
HERE

gnuplot $PLOT_NAME-time.plot
echo Generated $PLOT_NAME-time.eps


cp -f *.eps ../../eps
