#!/bin/bash


# TODO: granularity
# TODO: seqcalls

~/dev/evaluation/scripts/plotjob.sh dataparallel
~/dev/evaluation/scripts/plotjob.sh nested
~/dev/evaluation/scripts/plotjob.sh divconq
~/dev/evaluation/scripts/plotjob.sh divconq-n
gnuplot ~/dev/evaluation/scripts/divconq-usage.plot
gnuplot ~/dev/evaluation/scripts/pipeline-usage.plot

mkdir plots

for j in dataparallel nested divconq divconq-n; do
  for g in time speedup efficiency; do
    mv $j/plot/$g.eps plots/$j-$g.eps
  done
done
mv divconq-usage.eps plots
mv pipeline-usage.eps plots
