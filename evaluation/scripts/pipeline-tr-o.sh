#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.AddChar 5000
startshowload
echo "Startup completed"

export LOG_LEVEL=info
mkdir $JOB_DIR/local_logs
time nreduce $ELC_DIR/pipeline-tr.elc \
    1024 5000 `cat $JOB_DIR/computenodes` > $JOB_DIR/program.out 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs
