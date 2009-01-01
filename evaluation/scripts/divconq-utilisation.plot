#!/usr/bin/gnuplot
set terminal postscript eps solid color
set out "plots/eps/divconq-utilisation.eps"

set xlabel "Time (s)"
set ylabel "Utilization (%)"
set title "Divide & conquer, 32 nodes (64 processors)"
set yr [0:105]

plot "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n512/showload.log" \
     using 2:3 title "512 items" with lines linewidth 2, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n256/showload.log" \
     using 2:3 title "256 items" with lines linewidth 2, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n128/showload.log" \
     using 2:3 title "128 items" with lines linewidth 2, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n64/showload.log" \
     using 2:3 title "64 items" with lines linewidth 2, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n32/showload.log" \
     using 2:3 title "32 items" with lines linewidth 2
