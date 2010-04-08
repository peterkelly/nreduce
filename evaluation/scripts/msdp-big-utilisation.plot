set terminal postscript eps color size 15cm, 9cm
set out "plots/eps/msdp-big-utilisation.eps"
set size 0.75, 0.75

set xlabel "Time (s)"
set ylabel "Average processor utilisation (%)"
set yr [0:105]

set style line 1 linewidth 2 linecolor rgbcolor "red" linetype 1

plot "<grep total jobs/msdp-big-o/msdp-big-o.r0.n32/showload.log" \
     using 2:3 notitle with lines ls 1
