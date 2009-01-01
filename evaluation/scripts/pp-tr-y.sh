#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.AddChar 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $INITIAL run 0 $ELC_DIR/pp-tr.elc \
    50 5 256 localhost 5000 > $JOB_DIR/program.out
echo Program exited with status $?

shutdown
