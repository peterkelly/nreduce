#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startvm
startshowload
echo "Startup completed"

time nreduce --client $HOSTNAME run 0 $ELC_DIR/nfib.elc 40
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/logs
