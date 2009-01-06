#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <granularity>"
  exit 1
fi

GRANULARITY=$1

((requests=65536/$GRANULARITY))

echo GRANULARITY $GRANULARITY requests $requests mult $((requests*GRANULARITY))

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $INITIAL run 0 $ELC_DIR/dataparallel.elc localhost 5000 $requests $GRANULARITY
echo Program exited with status $?

shutdown
