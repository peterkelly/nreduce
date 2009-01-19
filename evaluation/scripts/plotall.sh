#!/bin/bash

SCRIPT_DIR=`dirname $0`

if [ ! -d jobs ]; then
  echo jobs directory not found. Check you are running this script from the correct location.
  exit 1
fi

$SCRIPT_DIR/plotworksize-p.sh jobs plots worksize-p "Execution time vs. work size"
$SCRIPT_DIR/plotpostpone.sh jobs plots postpone "Effect of postponing service connection frames"

$SCRIPT_DIR/plotnumcalls.sh jobs plots numcalls-o "Number of service calls"
$SCRIPT_DIR/plotnumcalls.sh jobs plots numcalls-hn "Number of service calls - HOSTNAME"
$SCRIPT_DIR/plotgranularity.sh jobs plots granularity "Granularity"

$SCRIPT_DIR/plotjob.sh jobs plots dataparallel "Data parallelism"
$SCRIPT_DIR/plotjob.sh jobs plots nested "Nested data parallelism"
$SCRIPT_DIR/plotjob.sh jobs plots divconq-comp "Divide & conquer"
$SCRIPT_DIR/plotjob.sh jobs plots pp-comp "Pseudo-pipeline"
$SCRIPT_DIR/plotjob.sh jobs plots msdp-comp "Multi-stage data parallelism"

gnuplot $SCRIPT_DIR/divconq-utilisation.plot
gnuplot $SCRIPT_DIR/pipeline-utilisation.plot
gnuplot $SCRIPT_DIR/pp-utilisation.plot

$SCRIPT_DIR/plotdata.sh jobs plots divconq-tr "Divide & conquer"
$SCRIPT_DIR/plotdata.sh jobs plots pipeline-tr "Pipeline"
$SCRIPT_DIR/plotdata.sh jobs plots pp-tr "Pseudo-pipeline"
$SCRIPT_DIR/plotdata.sh jobs plots msdp-tr "Multi-stage data parallelism"
