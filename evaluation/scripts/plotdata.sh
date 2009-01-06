#!/bin/bash

BASE=`pwd`

SCRIPT_DIR=`dirname $0`

if (($# < 4)); then
  echo "Usage: $0 <jobsdir> <plotdir> <plotname> <title>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3
TITLE=$4
JOB_NAMES="$PLOT_NAME-o $PLOT_NAME-y"

mkdir -p $PLOT_DIR/plots/$PLOT_NAME

OUTPUT_FILE=$PLOT_DIR/plots/$PLOT_NAME/$PLOT_NAME

cat >$OUTPUT_FILE.plot <<HERE
set terminal postscript eps color size 15.0cm, 10.0cm
set output "$PLOT_NAME.eps"

set xlabel "# nodes"
set ylabel "Data transferred between hosts (MB)"
set xtics 4
set format y "%4.f"
set title "$TITLE - data transfer"

set style data lines
set key bottom right
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "#00CC00" linetype 3
set style fill solid 0.2

set size 0.75, 0.75

plot "$PLOT_NAME-o.dat" using 1:(\$2/1024) title "Orchestration" with lines ls 1, \\
     "$PLOT_NAME-y.dat" using 1:(\$2/1024) title "Choreography" with lines ls 2, \\
     "$PLOT_NAME-y.dat" using 1:(\$3/1024) notitle with lines ls 3, \\
     "$PLOT_NAME-y.dat" using 1:(\$4/1024) notitle with lines ls 3
HERE


for JOB in $JOB_NAMES; do
  echo -n Getting data transfer statistics for $JOB
  cd $BASE/$JOBS_DIR/$JOB
  DATAFILE=$BASE/$PLOT_DIR/plots/$PLOT_NAME/$JOB.dat
  echo "# n avg min max" > $DATAFILE

  if echo $JOB | grep -qE -- '-o$'; then
    between="processes" # orchestration
  else
    between="machines" # choreography
  fi

  nvalues=`for i in *; do echo $i | sed -e 's/^.*n//'; done | sort -n | uniq`
  for n in $nvalues; do
    values=""
    for dir in *.r*.n$n; do
      data=`grep "Total data between $between: " $dir/logs/netstats | sed -e 's/^.*: //' -e 's/ .*$//'`
      values="$values$data "
    done
    avg=`average $values`
    min=`min $values`
    max=`max $values`
    echo -n .
    echo "$n $avg $min $max" >> $DATAFILE
  done
  echo
done

cd $BASE/$PLOT_DIR/plots/$PLOT_NAME
gnuplot $PLOT_NAME.plot
mkdir -p ../../eps
cp -f $PLOT_NAME.eps ../../eps
