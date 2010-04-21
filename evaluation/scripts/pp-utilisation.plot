set terminal postscript eps color size 15cm, 9cm
set out "plots/eps/pp-utilisation.eps"
set size 0.75, 0.75

set xlabel "Time (s)"
set ylabel "Average processor utilisation (%)"
set yr [0:105]
set xr [0:260]
set key bottom right

set style line 1 linewidth 2 linecolor rgbcolor "red" linetype 1
set style line 2 linewidth 2 linecolor rgbcolor "green" linetype 1
set style line 3 linewidth 2 linecolor rgbcolor "blue" linetype 1
set style line 4 linewidth 2 linecolor rgbcolor "violet" linetype 1
set style line 5 linewidth 2 linecolor rgbcolor "orange" linetype 1

plot \
 "< grep total jobs/pp-utilisation/pp-utilisation.r0.n50/showload.log" \
 using 2:3 title "50 items" with lines ls 1, \
 "< grep total jobs/pp-utilisation/pp-utilisation.r0.n40/showload.log" \
 using 2:3 title "40 items" with lines ls 2, \
 "< grep total jobs/pp-utilisation/pp-utilisation.r0.n30/showload.log" \
 using 2:3 title "30 items" with lines ls 3, \
 "< grep total jobs/pp-utilisation/pp-utilisation.r0.n20/showload.log" \
 using 2:3 title "20 items" with lines ls 4, \
 "< grep total jobs/pp-utilisation/pp-utilisation.r0.n10/showload.log" \
 using 2:3 title "10 items" with lines ls 5
