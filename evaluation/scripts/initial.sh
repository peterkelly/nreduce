#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <jobdir> <initial> <loglevel>"
  exit 1
fi

JOB_DIR=$1
INITIAL=$2
LOG_LEVEL=$3

if [ -e $JOB_DIR/env ]; then
  . $JOB_DIR/env
fi

if [ ! -z $LOG_LEVEL ]; then
  export LOG_LEVEL
fi

if [ ! -d $JOB_DIR ]; then
  echo "$JOB_DIR does not exist"
  exit 1
fi

export LOCAL_PORT_RANGE=49152-65535
#export EVENTS_DIR=$JOB_DIR/logs
export JOB_DIR
~/dev/nreduce/src/nreduce -w >$JOB_DIR/logs/$INITIAL.log 2>&1
