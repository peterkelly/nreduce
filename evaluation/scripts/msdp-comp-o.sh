#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload
echo "Startup completed"

time nreduce $ELC_DIR/msdp-comp.elc $HOSTNAME 5001 128 5 2500
echo Program exited with status $?

shutdown
