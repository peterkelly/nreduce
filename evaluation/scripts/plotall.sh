#!/bin/bash

SCRIPT_DIR=`dirname $0`

if [ ! -d jobs ]; then
  echo jobs directory not found. Check you are running this script from the correct location.
  exit 1
fi

$SCRIPT_DIR/ploteval.sh jobs plots
gnuplot $SCRIPT_DIR/eval-utilisation.plot

$SCRIPT_DIR/plotworksize-p.sh jobs plots worksize-p "Execution time vs. work size"
$SCRIPT_DIR/plotpostpone.sh jobs plots postpone "Effect of postponing service connection frames"

$SCRIPT_DIR/plotnumcalls.sh jobs plots numcalls-o "Task throughput vs. number of tasks"
#$SCRIPT_DIR/plotnumcalls.sh jobs plots numcalls-hn "Amount of parallelism - HOSTNAME"
$SCRIPT_DIR/plotgranularity.sh jobs plots granularity "Granularity"

$SCRIPT_DIR/plotjob.sh jobs plots dataparallel "Data parallelism"
$SCRIPT_DIR/plotjob.sh jobs plots nested "Nested data parallelism"
$SCRIPT_DIR/plotjob.sh jobs plots divconq-comp "Divide & conquer"
$SCRIPT_DIR/plotjob.sh jobs plots pp-comp "Logical pipelining"
$SCRIPT_DIR/plotmsdp-comp.sh jobs plots msdp-comp "Multi-stage data parallelism"

gnuplot $SCRIPT_DIR/divconq-utilisation.plot
gnuplot $SCRIPT_DIR/pipeline-utilisation.plot
gnuplot $SCRIPT_DIR/pp-utilisation.plot
gnuplot $SCRIPT_DIR/msdp-comp-utilisation.plot

$SCRIPT_DIR/plotdata.sh jobs plots divconq-tr "Divide & conquer"
$SCRIPT_DIR/plotdata.sh jobs plots pipeline-tr "Physical pipelining"
$SCRIPT_DIR/plotdata.sh jobs plots pp-tr "Logical pipelining"
$SCRIPT_DIR/plotdata-msdp.sh jobs plots msdp-tr "Multi-stage data parallelism"

$SCRIPT_DIR/plotjob.sh jobs plots marks-time "Student marks workflow"
$SCRIPT_DIR/plotdata.sh jobs plots marks-tr "Student marks workflow"
$SCRIPT_DIR/plotjob.sh jobs plots marks-fishhalf-time "Student marks workflow"
$SCRIPT_DIR/plotdata.sh jobs plots marks-fishhalf-tr "Student marks workflow"
$SCRIPT_DIR/plotscale.sh jobs plots
