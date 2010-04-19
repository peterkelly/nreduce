#!/bin/bash

BASE=`pwd`

SCRIPT_DIR=`dirname $0`

if (($# < 3)); then
  echo "Usage: $0 <jobsdir> <plotdir> <plotname>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
EXP_NAME=$3
EXTRA=$4
JOB_NAMES="$EXP_NAME-o $EXP_NAME-y"

mkdir -p $PLOT_DIR/plots/$EXP_NAME

OUTPUT_FILE=$PLOT_DIR/plots/$EXP_NAME/$EXP_NAME-data

cat >$OUTPUT_FILE.plot <<HERE
set terminal postscript eps color size 15.0cm, 9cm
set output "$EXP_NAME-data.eps"
set size 0.75, 0.75

set xlabel "Source & compiled code size (Kb)"
set ylabel "Data transferred between hosts (Mb)"
set xtics 64
set format y "%4.f"
#set xrange [8:128]

set style data lines
set key bottom right
set grid

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "#00CC00" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "#00CC00" linetype 3
set style line 4 linewidth 2 linecolor rgbcolor "#FF0000" linetype 1
set style fill solid 0.2

plot "$EXP_NAME-o-data.dat" using 1:(\$2/1024) title "Orchestration" with lines ls 1, \\
     "$EXP_NAME-y-data.dat" using 1:(\$2/1024) title "Choreography" with lines ls 2, \\
     "$EXP_NAME-y-data.dat" using 1:(\$3/1024) title "Choreography min/max" with lines ls 3, \\
     "$EXP_NAME-y-data.dat" using 1:(\$4/1024) notitle with lines ls 3 \
$EXTRA
HERE


for JOB in $JOB_NAMES; do
  echo -n Getting data transfer statistics for $JOB
  cd $BASE/$JOBS_DIR/$JOB
  DATAFILE=$BASE/$PLOT_DIR/plots/$EXP_NAME/$JOB-data.dat
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

cd $BASE/$PLOT_DIR/plots/$EXP_NAME
gnuplot $EXP_NAME-data.plot
mkdir -p ../../eps
cp -f $EXP_NAME-data.eps ../../eps
