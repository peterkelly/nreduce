#!/bin/bash


# TODO: granularity
# TODO: seqcalls

~/dev/evaluation/scripts/plotjob.sh jobs plots dataparallel dataparallel-o dataparallel-y
~/dev/evaluation/scripts/plotjob.sh jobs plots nested nested-o nested-y
~/dev/evaluation/scripts/plotjob.sh jobs plots divconq-comp divconq-comp-o divconq-comp-y
~/dev/evaluation/scripts/plotjob.sh jobs plots pp-comp pp-comp-o pp-comp-y

gnuplot ~/dev/evaluation/scripts/divconq-usage.plot
gnuplot ~/dev/evaluation/scripts/pipeline-usage.plot
gnuplot ~/dev/evaluation/scripts/pp-usage.plot
