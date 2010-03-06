#!/bin/bash

SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

. $SCRIPT_DIR/marks-time-params.sh

startcservice 'java -cp dev/evaluation/ws Marks' 5000
startvm
startshowload
echo "Startup completed"

cat $XQ_DIR/marks.xq \
  | sed -e "s/__HOSTNAME__/localhost/" \
  | sed -e "s/__PORT__/5000/" > $JOB_DIR/program.xq

$SCRIPT_DIR/../../xquery/runcompile $JOB_DIR/program.xq > $JOB_DIR/program.elc

time nreduce --client $INITIAL run 0 $JOB_DIR/program.elc
echo Program exited with status $?

shutdown
