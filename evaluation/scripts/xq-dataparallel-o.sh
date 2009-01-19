#!/bin/bash

SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

startcservice 'java -cp dev/evaluation/ws WebServices' 5000
startloadbal
startshowload
echo "Startup completed"

cat $XQ_DIR/dataparallel.xq \
  | sed -e "s/__HOSTNAME__/$HOSTNAME/" \
  | sed -e "s/__PORT__/5001/" > $JOB_DIR/program.xq

$SCRIPT_DIR/../../xquery/runcompile $JOB_DIR/program.xq > $JOB_DIR/program.elc

time nreduce $JOB_DIR/program.elc
echo Program exited with status $?

shutdown
