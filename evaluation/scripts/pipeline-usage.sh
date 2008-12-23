#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <size>"
  exit 1
fi

NITEMS=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 1234
startloadbal
startshowload_servicenodes
echo "Startup completed"

echo nreduce $ELC_DIR/pipeline.elc $NITEMS 10000 `grep -v $HOSTNAME $JOB_DIR/jobnodes`
time nreduce $ELC_DIR/pipeline.elc $NITEMS 10000 `grep -v $HOSTNAME $JOB_DIR/jobnodes`
echo Program exited with status $?

shutdown
