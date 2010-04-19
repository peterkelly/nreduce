#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compdata 5000
startvm
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

time nreduce --client $INITIAL run 0 $ELC_DIR/pipeline.elc \
    $NITEMS $COMPMS $PORT $ITEMSIZE $SERVICES \
    > $JOB_DIR/program.out
echo Program exited with status $?

shutdown
