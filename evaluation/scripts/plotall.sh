#!/bin/bash


# TODO: granularity
# TODO: seqcalls

~/dev/evaluation/scripts/plotjob.sh jobs plots dataparallel dataparallel-o dataparallel-y
~/dev/evaluation/scripts/plotjob.sh jobs plots nested nested-o nested-y
~/dev/evaluation/scripts/plotjob.sh jobs plots divconq-comp divconq-comp-o divconq-comp-y
~/dev/evaluation/scripts/plotjob.sh jobs plots pp-comp pp-comp-o pp-comp-y
~/dev/evaluation/scripts/plotjob.sh jobs plots msdp-comp msdp-comp-o msdp-comp-y

gnuplot ~/dev/evaluation/scripts/divconq-utilisation.plot
gnuplot ~/dev/evaluation/scripts/pipeline-utilisation.plot
gnuplot ~/dev/evaluation/scripts/pp-utilisation.plot

~/dev/evaluation/scripts/plotdata.sh jobs plots divconq-tr divconq-tr-o divconq-tr-y
~/dev/evaluation/scripts/plotdata.sh jobs plots pipeline-tr pipeline-tr-o pipeline-tr-y
~/dev/evaluation/scripts/plotdata.sh jobs plots pp-tr pp-tr-o pp-tr-y
~/dev/evaluation/scripts/plotdata.sh jobs plots msdp-tr msdp-tr-o msdp-tr-y
