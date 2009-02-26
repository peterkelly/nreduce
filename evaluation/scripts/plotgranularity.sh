#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 4)); then
  echo "Usage: $0 <jobsdir> <plotdir> <plotname> <title>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3
TITLE=$4

OUTPUT_DIR=$PLOT_DIR/plots/$PLOT_NAME

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$PLOT_NAME

# Extract + collate execution times from all runs
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-o $PLOT_NAME-o $OUTPUT_DIR/$PLOT_NAME-o.dat
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-y $PLOT_NAME-y $OUTPUT_DIR/$PLOT_NAME-y.dat

# Plot execution time
cd $OUTPUT_DIR

cat >$PLOT_NAME.plot <<HERE
set terminal postscript eps color size 15cm, 10cm
set output "$PLOT_NAME.eps"
set size 0.75, 0.75

set xlabel "# ms/call"
set ylabel "Execution time (s)"

set logscale x

#set ytics 2
#set ytics 16
set xtics 2
set xrange [4:2048]
set format y "%4.f"
set title "$TITLE"

set style data lines
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "#00CC00" linetype 3
set style fill solid 0.2

plot "$PLOT_NAME-o.dat" using 1:2 title "Orchestration" with lines ls 1, \\
     "$PLOT_NAME-y.dat" using 1:2 title "Choreography" with lines ls 2
HERE


gnuplot $PLOT_NAME.plot
echo Generated $PLOT_NAME.eps

cp -f *.eps ../../eps
