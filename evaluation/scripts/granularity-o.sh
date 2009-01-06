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
startloadbal
startshowload
echo "Startup completed"

export LOCAL_PORT_RANGE=49152-65535
time nreduce $ELC_DIR/dataparallel.elc $HOSTNAME 5001 $requests $GRANULARITY
echo Program exited with status $?

shutdown
