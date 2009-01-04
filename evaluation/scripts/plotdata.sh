#!/bin/bash

BASE=`pwd`

SCRIPT_DIR=`dirname $0`

if (($# < 3)); then
  echo "Usage: $0 <jobdir> <plotdir> <plotname> <jobnames...>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3
shift 3
JOB_NAMES=$@

mkdir -p $PLOT_DIR/plots/$PLOT_NAME

OUTPUT_FILE=$PLOT_DIR/plots/$PLOT_NAME/$PLOT_NAME

cat >$OUTPUT_FILE.plot <<HERE
#!/usr/bin/gnuplot
set terminal postscript eps solid color
set out "$PLOT_NAME.eps"

set xlabel "# nodes"
set ylabel "Data (MB)"
HERE

echo -n "plot " >> $OUTPUT_FILE.plot

count=0

for JOB in $JOB_NAMES; do
  if ((count>0)); then
    echo ", \\" >> $OUTPUT_FILE.plot
  fi
  echo -n \"$JOB.dat\" using 1:2 title \"$JOB\" with lines >> $OUTPUT_FILE.plot
  ((count++))
done
echo "" >> $OUTPUT_FILE.plot
#gnuplot $OUTPUT_FILE.plot


for JOB in $JOB_NAMES; do
  cd $BASE/$JOBS_DIR/$JOB
  DATAFILE=$BASE/$PLOT_DIR/plots/$PLOT_NAME/$JOB.dat
  echo "# n avg min max" > $DATAFILE
  echo ========== $JOB =============

  echo $JOB | grep -qE -- '-o$'
  orch=$?

  nvalues=`for i in *; do echo $i | sed -e 's/^.*n//'; done | sort -n | uniq`
  echo $nvalues
  for n in $nvalues; do
    values=""
    for dir in *.r*.n$n; do
      if [ $orch -eq 0 ]; then
        data=`grep 'Total data between processes: ' $dir/logs/netstats | sed -e 's/^.*: //' -e 's/ .*$//'`
      else
        data=`grep 'Total data between machines: ' $dir/logs/netstats | sed -e 's/^.*: //' -e 's/ .*$//'`
      fi
      values="$values$data "
    done
    avg=`~/dev/tools/average $values`
    min=`~/dev/tools/min $values`
    max=`~/dev/tools/max $values`
    echo "n = $n, avg = $avg, values = $values"
    echo "$n $avg $min $max" >> $DATAFILE
  done
done

cd $BASE/$PLOT_DIR/plots/$PLOT_NAME
gnuplot $PLOT_NAME.plot
mkdir -p ../../eps
cp -f $PLOT_NAME.eps ../../eps
