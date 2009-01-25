#!/bin/bash

SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

echo "export NSTUDENTS=1" > $JOB_DIR/env
echo "export NTESTS=1024" >> $JOB_DIR/env
echo "export TESTMS=500" >> $JOB_DIR/env

startcservice 'java -cp dev/evaluation/ws Marks' 5000
startloadbal
startshowload
echo "Startup completed"

cat $XQ_DIR/marks.xq \
  | sed -e "s/__HOSTNAME__/$HOSTNAME/" \
  | sed -e "s/__PORT__/5001/" > $JOB_DIR/program.xq

$SCRIPT_DIR/../../xquery/runcompile $JOB_DIR/program.xq > $JOB_DIR/program.elc

time nreduce $JOB_DIR/program.elc
echo Program exited with status $?

shutdown
