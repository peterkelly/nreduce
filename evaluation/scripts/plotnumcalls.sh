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

msvalues=`cd $JOBS_DIR/$PLOT_NAME && for i in $PLOT_NAME.*; do echo $i; \
          done | sed -e 's/^.*ms\(.*\).r.*$/\1/' | sort -n | uniq`

# Extract + collate execution times from all runs
for ms in $msvalues; do
  $SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME $PLOT_NAME.ms$ms $OUTPUT_DIR/$PLOT_NAME.ms$ms.dat
done

# Plot execution time
cd $OUTPUT_DIR

cat >$PLOT_NAME.plot <<HERE
set terminal postscript eps color size 15cm, 9cm
set output "$PLOT_NAME.eps"
set size 0.75, 0.75

set xlabel "# calls"
set ylabel "Average # calls per second"

set logscale x
unset mxtics
unset mytics
set key top left

set xtics 2
set xrange [:2097152]
set xtics rotate by 90
set format x "%7.f"
set format y "%4.f"

set style data lines
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "#00CCCC" linetype 1
set style line 4 linewidth 2 linecolor rgbcolor "red" linetype 1
set style fill solid 0.2

plot \\
HERE

count=1
for ms in $msvalues; do
  if ((count > 1)); then
    echo ", \\" >> $PLOT_NAME.plot
  fi
  echo -n "\"$PLOT_NAME.ms$ms.dat\" using 1:(\$1/\$2) notitle with lines ls $count" >> $PLOT_NAME.plot
  ((count++))
done

gnuplot $PLOT_NAME.plot
echo Generated $PLOT_NAME.eps

cp -f *.eps ../../eps
