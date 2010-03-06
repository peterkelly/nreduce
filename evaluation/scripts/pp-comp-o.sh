#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compute 5000
startloadbal
startshowload
echo "Startup completed"

NITEMS=256
COMPMS=1000
PORT=5001
SERVICES="$HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME $HOSTNAME"

export LOG_LEVEL=info
mkdir $JOB_DIR/local_logs
time nreduce $ELC_DIR/pipeline.elc \
    $NITEMS $COMPMS $PORT $SERVICES 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs
