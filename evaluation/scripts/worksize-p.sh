#!/bin/bash

if (($# < 2)); then
  echo "Usage: $o <compms> <fishframes>"
  exit 1
fi

COMPMS=$1
echo "export OPT_FISHFRAMES=$2" > $JOB_DIR/env

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc
#export LOG_LEVEL=debug2

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $INITIAL run 0 $ELC_DIR/dataparallel.elc localhost 5000 4096 $COMPMS
echo Program exited with status $?

shutdown
