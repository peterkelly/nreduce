set terminal postscript eps color
set out "/Users/peter/evaluation/test2/plots/hist.eps"
set key left
set ylabel "User time (s)"
set title "All programs"
set style data histograms
set style fill solid 1.0 border -1
set xtics rotate by 90
set yrange [0:260]
plot \
  "/Users/peter/evaluation/test2/plots/hist.dat" using 2 : xticlabels(1) title "c", \
  "/Users/peter/evaluation/test2/plots/hist.dat" using 3 : xticlabels(1) title "elc", \
  "/Users/peter/evaluation/test2/plots/hist.dat" using 4 : xticlabels(1) title "java", \
  "/Users/peter/evaluation/test2/plots/hist.dat" using 5 : xticlabels(1) title "js", \
  "/Users/peter/evaluation/test2/plots/hist.dat" using 6 : xticlabels(1) title "perl", \
  "/Users/peter/evaluation/test2/plots/hist.dat" using 7 : xticlabels(1) title "python"
