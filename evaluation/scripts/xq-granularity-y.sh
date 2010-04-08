#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <granularity>"
  exit 1
fi

GRANULARITY=$1

((requests=65536/$GRANULARITY))

echo GRANULARITY $GRANULARITY requests $requests mult $((requests*GRANULARITY))

SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

startcservice 'java -cp dev/evaluation/ws Compute' 5000
startvm
startshowload
echo "Startup completed"

cat $XQ_DIR/compute.xq \
  | sed -e "s/localhost:5000/localhost:5000/" \
  | sed -e "s/size := .*;/size := $requests;/" \
  | sed -e "s/compms := .*;/compms := $GRANULARITY;/" > $JOB_DIR/program.xq

echo Before compile: `date`
$SCRIPT_DIR/../../xquery/runcompile $JOB_DIR/program.xq > $JOB_DIR/program.elc
echo After compile: `date`

time nreduce --client $INITIAL run 0 $JOB_DIR/program.elc
echo Program exited with status $?
echo After execution: `date`

shutdown
