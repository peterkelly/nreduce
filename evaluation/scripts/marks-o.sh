#!/bin/bash

SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

. $SCRIPT_DIR/marks-params.sh

startcservice 'java -cp dev/evaluation/ws Marks' 5000
startloadbal
startshowload
echo "Startup completed"

cat $XQ_DIR/marks.xq \
  | sed -e "s/__HOSTNAME__/$HOSTNAME/" \
  | sed -e "s/__PORT__/5001/" > $JOB_DIR/program.xq

$SCRIPT_DIR/../../xquery/runcompile $JOB_DIR/program.xq > $JOB_DIR/program.elc

export LOG_LEVEL=info
mkdir $JOB_DIR/local_logs
time nreduce $JOB_DIR/program.elc > $JOB_DIR/program.out 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs > $JOB_DIR/logs/netstats
