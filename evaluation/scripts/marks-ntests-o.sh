#!/bin/bash

NTESTS=$1
SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

echo "export NSTUDENTS=64" > $JOB_DIR/env
echo "export NTESTS=$NTESTS" >> $JOB_DIR/env
echo "export TESTMS=1000" >> $JOB_DIR/env
echo "export CODESIZE=131072" >> $JOB_DIR/env
echo "export SOURCESIZE=131072" >> $JOB_DIR/env
echo "export INOUTSIZE=10" >> $JOB_DIR/env

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
time nreduce $JOB_DIR/program.elc 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs > $JOB_DIR/logs/netstats
