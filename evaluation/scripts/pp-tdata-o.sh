#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <isize>"
  exit 1
fi

ISIZE=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.AddChar 5000
startloadbal
startshowload
echo "Startup completed"

export LOG_LEVEL=info
mkdir $JOB_DIR/local_logs
time nreduce $ELC_DIR/pp-tr.elc \
    50 5 $ISIZE $HOSTNAME 5001 > $JOB_DIR/program.out 2>$JOB_DIR/local_logs/$HOSTNAME.log
echo Program exited with status $?

shutdown

java -cp $SCRIPT_DIR/.. util.CollateNetStats $JOB_DIR/local_logs > $JOB_DIR/logs/netstats
