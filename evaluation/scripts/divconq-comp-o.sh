#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload
echo "Startup completed"

time nreduce $ELC_DIR/divconq-comp.elc $HOSTNAME 5001 512 1000
echo Program exited with status $?

shutdown
