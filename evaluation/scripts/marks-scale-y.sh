#!/bin/bash

MS=$1

if [ -z "$MS" ]; then
  echo "Error: ms must be specified"
  exit 1
fi

SCRIPT_DIR=`dirname $0`
XQ_DIR=$SCRIPT_DIR/../xquery

. $SCRIPT_DIR/common.sh

echo "export NSTUDENTS=64" > $JOB_DIR/env
echo "export NTESTS=64" >> $JOB_DIR/env
echo "export TESTMS=${MS}" >> $JOB_DIR/env
echo "export CODESIZE=131072" >> $JOB_DIR/env
echo "export SOURCESIZE=131072" >> $JOB_DIR/env
echo "export INOUTSIZE=10" >> $JOB_DIR/env

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
