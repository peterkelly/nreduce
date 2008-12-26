#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 1234
startloadbal
startshowload
echo "Startup completed"

time nreduce $ELC_DIR/nested.elc $HOSTNAME 1235 32 1000
echo Program exited with status $?

shutdown
