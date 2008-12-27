#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <size>"
  exit 1
fi

NITEMS=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload_servicenodes
echo "Startup completed"

time nreduce $ELC_DIR/pipeline.elc $NITEMS 10000 5000 `grep -v $HOSTNAME $JOB_DIR/jobnodes`
echo Program exited with status $?

shutdown
