#!/bin/bash

SCRIPT_DIR=`dirname $0`

if [ ! -d jobs ]; then
  echo jobs directory not found. Check you are running this script from the correct location.
  exit 1
fi

$SCRIPT_DIR/ploteval.sh jobs plots
gnuplot $SCRIPT_DIR/eval-utilisation.plot

$SCRIPT_DIR/plotworksize-p.sh jobs plots worksize-p
$SCRIPT_DIR/plotpostpone.sh jobs plots postpone

$SCRIPT_DIR/plotnumcalls.sh jobs plots numcalls-o
#$SCRIPT_DIR/plotnumcalls.sh jobs plots numcalls-hn
$SCRIPT_DIR/plotgranularity.sh jobs plots granularity

$SCRIPT_DIR/plotjob.sh jobs plots dataparallel
$SCRIPT_DIR/plotjob.sh jobs plots nested
$SCRIPT_DIR/plotjob.sh jobs plots divconq-comp
$SCRIPT_DIR/plotjob.sh jobs plots pp-comp
$SCRIPT_DIR/plotmsdp-comp.sh jobs plots msdp-comp

gnuplot $SCRIPT_DIR/divconq-utilisation.plot
gnuplot $SCRIPT_DIR/pipeline-utilisation.plot
gnuplot $SCRIPT_DIR/pp-utilisation.plot
gnuplot $SCRIPT_DIR/msdp-comp-utilisation.plot

$SCRIPT_DIR/plotdata.sh jobs plots divconq-tr
$SCRIPT_DIR/plotdata.sh jobs plots pipeline-tr
$SCRIPT_DIR/plotdata.sh jobs plots pp-tr
$SCRIPT_DIR/plotdata-msdp.sh jobs plots msdp-tr

$SCRIPT_DIR/plotjob.sh jobs plots marks-time
$SCRIPT_DIR/plotdata.sh jobs plots marks-tr
$SCRIPT_DIR/plotjob.sh jobs plots marks-fishhalf-time
$SCRIPT_DIR/plotdata.sh jobs plots marks-fishhalf-tr
$SCRIPT_DIR/plotscale.sh jobs plots
