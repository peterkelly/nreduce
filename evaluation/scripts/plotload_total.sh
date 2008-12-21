#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <showload-log> <usage>"
  exit 1
fi

SHOWLOAD_LOG=$1
OUTPUT_FILE=$2

cat >$OUTPUT_FILE.plot <<HERE
#!/usr/bin/gnuplot
set terminal postscript eps solid color
set out "$OUTPUT_FILE"

set xlabel "Time (s)"
set ylabel "Utilization (%)"
set yr [0:105]
HERE

echo -n "plot " >> $OUTPUT_FILE.plot

echo -n \"\< grep total $SHOWLOAD_LOG\" using 2:3 title \'$i\' with lines " " linewidth 2 >> $OUTPUT_FILE.plot
echo "" >> $OUTPUT_FILE.plot
gnuplot $OUTPUT_FILE.plot
