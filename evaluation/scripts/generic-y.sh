#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc
PARAMFILE=$1

if [ ! -s "$PARAMFILE" ]; then
  echo $PARAMFILE not found
  exit 1
fi

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compdata 5000
startvm
startshowload
echo "Startup completed"

HOST=localhost
PORT=5000

. $PARAMFILE

echo PROGRAM $PROGRAM
echo ARGS $ARGS

time nreduce --client $INITIAL run 0 $PROGRAM $ARGS >$JOB_DIR/program.out 2>$JOB_DIR/program.err
echo Program exited with status $?

shutdown
