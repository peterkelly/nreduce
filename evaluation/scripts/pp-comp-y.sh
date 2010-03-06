#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startvm
startshowload
echo "Startup completed"

NITEMS=256
COMPMS=1000
PORT=5000
SERVICES="localhost localhost localhost localhost localhost"

time nreduce --client $INITIAL run 0 $ELC_DIR/pipeline.elc \
    $NITEMS $COMPMS $PORT $SERVICES
echo Program exited with status $?

shutdown
