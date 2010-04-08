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
$SCRIPT_DIR/plotgranularity.sh jobs plots xq-granularity

$SCRIPT_DIR/plotjob2.sh jobs plots dataparallel "1 sec, 0 Kb" "5 sec, 256 Kb"
$SCRIPT_DIR/plotjob2.sh jobs plots nested "1 sec, 0 Kb" "5 sec, 256 Kb"
$SCRIPT_DIR/plotjob2.sh jobs plots divconq "1 sec, 0 Kb" "5 sec, 64 Kb"
$SCRIPT_DIR/plotjob2.sh jobs plots pp "1 sec, 0 Kb" "5 sec, 256 Kb"
$SCRIPT_DIR/plotmsdp-comp.sh jobs plots msdp "2.5 sec, 0 Kb" "5 sec, 64 Kb"

gnuplot $SCRIPT_DIR/divconq-utilisation.plot
gnuplot $SCRIPT_DIR/pipeline-utilisation.plot
gnuplot $SCRIPT_DIR/pp-utilisation.plot
gnuplot $SCRIPT_DIR/msdp-small-utilisation.plot

$SCRIPT_DIR/plotdata.sh jobs plots dataparallel-big
$SCRIPT_DIR/plotdata.sh jobs plots nested-big
$SCRIPT_DIR/plotdata.sh jobs plots divconq-big
$SCRIPT_DIR/plotdata.sh jobs plots pipeline-tr
$SCRIPT_DIR/plotdata.sh jobs plots pp-big
$SCRIPT_DIR/plotdata-msdp.sh jobs plots msdp-big

$SCRIPT_DIR/plotjob.sh jobs plots marks
$SCRIPT_DIR/plotdata.sh jobs plots marks
$SCRIPT_DIR/plotjob.sh jobs plots marks-fishhalf-time
$SCRIPT_DIR/plotdata.sh jobs plots marks-fishhalf-tr
$SCRIPT_DIR/plotscale.sh jobs plots
