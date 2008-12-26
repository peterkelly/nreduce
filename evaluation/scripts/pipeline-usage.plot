set terminal postscript eps solid color
set out "pipeline-usage.eps"

set xlabel "Time (s)"
set ylabel "Utilization (%)"
set title "Pipeline with 8 stages"
set yr [0:100]

plot \
 "< grep total pipeline-usage/pipeline-usage.r0.n50/showload.log" \
 using 2:3 title "50 items" with lines linewidth 2, \
 "< grep total pipeline-usage/pipeline-usage.r0.n40/showload.log" \
 using 2:3 title "40 items" with lines linewidth 2, \
 "< grep total pipeline-usage/pipeline-usage.r0.n30/showload.log" \
 using 2:3 title "30 items" with lines linewidth 2, \
 "< grep total pipeline-usage/pipeline-usage.r0.n20/showload.log" \
 using 2:3 title "20 items" with lines linewidth 2, \
 "< grep total pipeline-usage/pipeline-usage.r0.n10/showload.log" \
 using 2:3 title "10 items" with lines linewidth 2
