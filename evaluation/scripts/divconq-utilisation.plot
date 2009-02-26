set terminal postscript eps color size 15cm, 10cm
set out "plots/eps/divconq-utilisation.eps"
set size 0.75, 0.75

set xlabel "Time (s)"
set ylabel "Utilization (%)"
set title "Divide & conquer, 32 nodes (64 processors)"
set yr [0:105]

set style line 1 linewidth 2 linecolor rgbcolor "red" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "green" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 4 linewidth 2 linecolor rgbcolor "magenta" linetype 1
set style line 5 linewidth 2 linecolor rgbcolor "cyan" linetype 1

plot "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n512/showload.log" \
     using 2:3 title "512 items" with lines ls 1, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n256/showload.log" \
     using 2:3 title "256 items" with lines ls 2, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n128/showload.log" \
     using 2:3 title "128 items" with lines ls 3, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n64/showload.log" \
     using 2:3 title "64 items" with lines ls 4, \
     "<grep total jobs/divconq-utilisation/divconq-utilisation.r0.n32/showload.log" \
     using 2:3 title "32 items" with lines ls 5
