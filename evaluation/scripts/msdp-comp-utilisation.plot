set terminal postscript eps color size 15cm, 10cm
set out "plots/eps/msdp-comp-utilisation.eps"
set size 0.75, 0.75

set xlabel "Time (s)"
set ylabel "Utilization (%)"
set title "Multi-stage data parallelism, 32 nodes (64 processors)"
set yr [0:105]

set style line 1 linewidth 2 linecolor rgbcolor "red" linetype 1

plot "<grep total jobs/msdp-comp-o/msdp-comp-o.r0.n32/showload.log" \
     using 2:3 notitle with lines ls 1
