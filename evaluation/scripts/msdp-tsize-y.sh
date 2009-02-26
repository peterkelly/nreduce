#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <nstages>"
  exit 1
fi

NSTAGES=$1

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.ProcessCombine 5000
startvm
startshowload
echo "Startup completed"

time nreduce --client $INITIAL run 0 $ELC_DIR/msdp-tr.elc \
    localhost 5000 128 $NSTAGES 64 > $JOB_DIR/program.out
echo Program exited with status $?

shutdown
