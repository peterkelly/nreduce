#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 3)); then
  echo "Usage: $0 <jobsdir> <plotdir> <plotname>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3

OUTPUT_DIR=$PLOT_DIR/plots/$PLOT_NAME

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$PLOT_NAME

xvalues=`cd $JOBS_DIR/$PLOT_NAME && for i in $PLOT_NAME.*; do echo $i; \
         done | sed -e 's/^.*x\(.*\).r\(.*\).n.*$/\1/' | sort -n | uniq`

# Extract + collate execution times from all runs
for x in $xvalues; do
  $SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME $PLOT_NAME.x$x $OUTPUT_DIR/$PLOT_NAME.x$x.dat
done

# Plot execution time
cd $OUTPUT_DIR

cat >$PLOT_NAME.plot <<HERE
set terminal postscript eps color size 15cm, 9cm
set output "$PLOT_NAME.eps"
set size 0.75, 0.75

set xlabel "# frames distributed per work request"
set ylabel "Relative execution time t/min(t)"

set logscale x
unset mxtics
unset mytics
set key top right

set xtics 4
set xrange [:4096]
set yrange [0.9:]
set ytics 1, 0.5
set format y "%3.1f"

set style data lines
set grid

set style fill solid 0.2

set style line 1 linewidth 2 linecolor rgbcolor "red" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "green" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 4 linewidth 2 linecolor rgbcolor "magenta" linetype 1
set style line 5 linewidth 2 linecolor rgbcolor "#00CCCC" linetype 1
set style line 6 linewidth 2 linecolor rgbcolor "#808000" linetype 1

plot \\
HERE

count=1
for x in $xvalues; do
  globalmin=`cat $PLOT_NAME.x$x.dat.globalmin`
  if ((count > 1)); then
    echo ", \\" >> $PLOT_NAME.plot
  fi
  echo -n "\"$PLOT_NAME.x$x.dat\" using 1:(\$2/$globalmin) title \"$x ms/call\" with lines ls $count" >> $PLOT_NAME.plot
  ((count++))
done

gnuplot $PLOT_NAME.plot
echo Generated $PLOT_NAME.eps

cp -f *.eps ../../eps
