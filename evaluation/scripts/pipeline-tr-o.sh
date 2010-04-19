#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compdata 5000
startshowload
echo "Startup completed"

NITEMS=50
COMPMS=1000
PORT=5000
SERVICES=`grep -v $HOSTNAME $JOB_DIR/jobnodes`
ITEMSIZE=65536

echo NITEMS $NITEMS
echo COMPMS $COMPMS
echo PORT $PORT
echo ITEMSIZE $ITEMSIZE
echo SERVICES $SERVICES

export LOG_LEVEL=info
mkdir $JOB_DIR/local_logs
time nreduce $ELC_DIR/pipeline.elc \
    $NITEMS $COMPMS $PORT $ITEMSIZE $SERVICES \
    > $JOB_DIR/program.out 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs > $JOB_DIR/logs/netstats
