#!/bin/bash


# TODO: granularity
# TODO: seqcalls

~/dev/evaluation/scripts/plotjob.sh jobs plots dataparallel dataparallel dataparallel-c
~/dev/evaluation/scripts/plotjob.sh jobs plots nested nested
~/dev/evaluation/scripts/plotjob.sh jobs plots divconq divconq
~/dev/evaluation/scripts/plotjob.sh jobs plots divconq-n divconq-n
~/dev/evaluation/scripts/plotjob.sh jobs plots pseudo-pipeline pseudo-pipeline-o pseudo-pipeline-c

gnuplot ~/dev/evaluation/scripts/divconq-usage.plot
gnuplot ~/dev/evaluation/scripts/pipeline-usage.plot
gnuplot ~/dev/evaluation/scripts/pseudo-pipeline-usage.plot
