#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $HOSTNAME run 0 $ELC_DIR/dataparallel.elc localhost 5000 1024 1000
echo Program exited with status $?

shutdown
