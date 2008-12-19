#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <jobdir> <initial> <loglevel>"
  exit 1
fi

JOB_DIR=$1
INITIAL=$2
LOG_LEVEL=$3

if [ ! -z $LOG_LEVEL ]; then
  export LOG_LEVEL
fi

if [ ! -d $JOB_DIR ]; then
  echo "$JOB_DIR does not exist"
  exit 1
fi

~/dev/nreduce/src/nreduce -w -i $INITIAL 1>$JOB_DIR/logs/$HOSTNAME.log 2>&1
