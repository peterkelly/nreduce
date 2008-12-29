#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.AddChar 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $HOSTNAME run 0 $ELC_DIR/transfer1.elc \
    1024 5000 `cat $JOB_DIR/jobnodes` > $JOB_DIR/program.out
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/logs
