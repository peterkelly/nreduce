#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <size>"
  exit 1
fi

SIZE=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload

echo "Startup completed"

time nreduce $ELC_DIR/divconq-comp.elc $HOSTNAME 5001 $SIZE 10000
echo Program exited with status $?

shutdown
