#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <size> <nstages>"
  exit 1
fi

NITEMS=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload
echo "Startup completed"

COMPMS=10000
PORT=5001
SERVICES="$HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME"

time nreduce $ELC_DIR/pipeline.elc $NITEMS $COMPMS $PORT $SERVICES
echo Program exited with status $?

shutdown
