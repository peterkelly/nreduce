#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <size> <nstages>"
  exit 1
fi

NITEMS=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compdata 5000
startloadbal
startshowload
echo "Startup completed"

COMPMS=10000
PORT=5001
SERVICES="$HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME"
ITEMSIZE=65536

echo NITEMS $NITEMS
echo COMPMS $COMPMS
echo PORT $PORT
echo ITEMSIZE $ITEMSIZE
echo SERVICES $SERVICES

time nreduce $ELC_DIR/pipeline.elc $NITEMS $COMPMS $PORT $ITEMSIZE $SERVICES
echo Program exited with status $?

shutdown
