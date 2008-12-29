#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <size> <nstages>"
  exit 1
fi

NITEMS=$1
NSTAGES=$2

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload_servicenodes
echo "Startup completed"

time nreduce $ELC_DIR/pseudo-pipeline.elc $NITEMS $NSTAGES 10000 $HOSTNAME 5001
echo Program exited with status $?

shutdown
