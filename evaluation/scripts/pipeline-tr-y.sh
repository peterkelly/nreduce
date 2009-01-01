#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.AddChar 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $INITIAL run 0 $ELC_DIR/pipeline-tr.elc \
    1024 5000 `cat $JOB_DIR/computenodes` > $JOB_DIR/program.out
echo Program exited with status $?

shutdown
