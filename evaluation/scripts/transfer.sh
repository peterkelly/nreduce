#!/bin/bash

SCRIPT_DIR=`dirname $0`

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compdata 5000
startloadbal
startshowload
echo "Startup completed"

time ~/dev/tools/transfer localhost 5001 1024 262144 64
echo Program exited with status $?

shutdown
