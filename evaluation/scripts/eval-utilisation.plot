set terminal postscript eps color size 15cm, 9cm
set out "plots/eps/eval-utilisation.eps"
set size 0.75, 0.75

set xlabel "Time (s)"
set ylabel "Average processor utilisation (%)"
set yr [0:105]

set style line 1 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "red" linetype 1

set size 0.75, 0.75

plot "<grep total jobs/eval-lazy/eval-lazy.r0.n4/showload.log" \
     using 2:($3*2) title "Lazy evaluation" with lines ls 1, \
     "<grep total jobs/eval-strict/eval-strict.r0.n4/showload.log" \
     using 2:($3*2) title "Strict evaluation" with lines ls 2
