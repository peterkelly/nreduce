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
gettimes.sh $JOBS_DIR/worksize-p worksize-p.x128 $PLOT_DIR/plots/$PLOT_NAME/worksize-p.x128.dat
gettimes.sh $JOBS_DIR/worksize-np worksize-np.x128 $PLOT_DIR/plots/$PLOT_NAME/worksize-np.x128.dat

# Plot execution time
cd $OUTPUT_DIR

cat >$PLOT_NAME.plot <<HERE
set terminal postscript eps color size 15cm, 10cm
set output "$PLOT_NAME.eps"

set xlabel "# frames distributed per work request"
set ylabel "Execution time (s)"

set logscale x
unset mxtics
unset mytics
set key top left

set xtics 4
set xrange [1:4096]
set yrange [0:]
set format y "%4.f"
set title "$TITLE"

set style data lines
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "blue" linetype 2
set style line 3 linewidth 2 linecolor rgbcolor "red" linetype 1
set style line 4 linewidth 2 linecolor rgbcolor "red" linetype 2
set style line 5 linewidth 2 linecolor rgbcolor "green" linetype 1
set style line 6 linewidth 2 linecolor rgbcolor "green" linetype 2
set style fill solid 0.2

set size 0.75, 0.75

plot "worksize-np.x128.dat" using 1:2 title "Postpone disabled" with lines ls 1, \\
     "worksize-np.x128.dat" using 1:3 notitle with lines ls 2, \\
     "worksize-np.x128.dat" using 1:4 notitle with lines ls 2, \\
     "worksize-p.x128.dat" using 1:2 title "Postpone enabled" with lines ls 3, \\
     "worksize-p.x128.dat" using 1:3 notitle with lines ls 4, \\
     "worksize-p.x128.dat" using 1:4 notitle with lines ls 4

HERE

gnuplot $PLOT_NAME.plot
echo Generated $PLOT_NAME.eps

cp -f *.eps ../../eps
