#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $HOSTNAME run 0 $ELC_DIR/pseudo-pipeline.elc \
    50 5 10000 localhost 5000
echo Program exited with status $?

shutdown
