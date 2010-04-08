#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc
PARAMFILE=$1

if [ ! -s "$PARAMFILE" ]; then
  echo $PARAMFILE not found
  exit 1
fi

. $SCRIPT_DIR/common.sh

startcservice dev/tools/svc_compdata 5000
startloadbal
startshowload
echo "Startup completed"

HOST=$HOSTNAME
PORT=5001

. $PARAMFILE

echo PROGRAM $PROGRAM
echo ARGS $ARGS

export LOG_LEVEL=info
mkdir $JOB_DIR/local_logs
time nreduce $PROGRAM $ARGS >/dev/null 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs > $JOB_DIR/logs/netstats
