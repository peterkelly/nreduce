#!/bin/bash

if (($# < 2)); then
  echo "Usage: $0 <jobdir> args..."
  exit 1
fi

JOB_DIR=$1
shift 1
ARGS=$@

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

$ARGS
