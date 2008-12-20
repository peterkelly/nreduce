#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <granularity>"
  exit 1
fi

GRANULARITY=$1

((requests=10000/$GRANULARITY))

echo GRANULARITY $GRANULARITY requests $requests mult $((requests*GRANULARITY))

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh



startup services.Compute 1234

time nreduce $ELC_DIR/basicmap.elc $HOSTNAME 1235 $requests $GRANULARITY
echo Program exited with status $?

shutdown
