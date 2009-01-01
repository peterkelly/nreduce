#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $INITIAL run 0 $ELC_DIR/divconq-comp.elc localhost 5000 512 10000
echo Program exited with status $?

shutdown
