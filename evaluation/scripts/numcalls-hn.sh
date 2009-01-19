#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <numcalls> <compms>"
  exit 1
fi

NUMCALLS=$1
COMPMS=$2

echo NUMCALLS $NUMCALLS

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

cd ~
dev/tools/svc_compute 5000 > $JOB_DIR/svc_compute.log 2>&1 &
echo "Startup completed"

export LOCAL_PORT_RANGE=49152-65535
export LOG_LEVEL=info
time nreduce $ELC_DIR/dataparallel.elc $HOSTNAME 5000 $NUMCALLS $COMPMS 2>$JOB_DIR/nreduce.log
echo Program exited with status $?

killall -9 svc_compute

shutdown
