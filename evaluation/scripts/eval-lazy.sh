#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice 'dev/tools/svc_compute -m 1' 5000
startloadbal
startshowload
echo "Startup completed"

time nreduce -v lazy $ELC_DIR/eval.elc $HOSTNAME 5001 256 1000
echo Program exited with status $?

shutdown
