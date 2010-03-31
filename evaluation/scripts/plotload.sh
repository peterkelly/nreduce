#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <showload-log> <usage>"
  exit 1
fi

SHOWLOAD_LOG=$1
OUTPUT_FILE=$2
MACHINES=`awk '{print $1}' $SHOWLOAD_LOG | sort | uniq`

cat >$OUTPUT_FILE.plot <<HERE
#!/usr/bin/gnuplot
set terminal postscript eps solid color
set out "$OUTPUT_FILE"

set xlabel "Time (s)"
set ylabel "Processor utilisation (%)"
set yr [0:105]
HERE

echo -n "plot " >> $OUTPUT_FILE.plot

count=0

for i in $MACHINES; do
  if ((count>0)); then
    echo -n , >> $OUTPUT_FILE.plot
  fi
  echo -n \"\< grep $i $SHOWLOAD_LOG\" using 2:3 title \'$i\' with lines " " >> $OUTPUT_FILE.plot
  ((count++))
done
echo "" >> $OUTPUT_FILE.plot
gnuplot $OUTPUT_FILE.plot
