#!/bin/bash

. ~/dev/evaluation/scripts/common.sh

if [ -z $JOB_DIR ]; then
  echo JOB_DIR is not set!
  exit 1
fi

NUM_NODES=`wc -l $JOB_DIR/jobnodes | cut -d ' ' -f 1`

echo "NUM_NODES = $NUM_NODES"

if [ $NUM_NODES -ne 5 ]; then
  echo This script requires exactly 5 nodes; $NUM_NODES are present
  exit 1
fi

SERVERS=`sort $JOB_DIR/jobnodes | head -4`
LAST=`sort $JOB_DIR/jobnodes | tail -1`

export LOG_LEVEL=debug2
echo "-------------------------------- Starting Servers and VM"
parsh -h $JOB_DIR/jobnodes 'killall -9 java nreduce'
runvm
runservice services.AddChar 1234
sleep 5
echo "-------------------------------- Running program"
nreduce --client $LAST run 0 ~/dev/evaluation/elc/pipeline.elc $SERVERS
echo "-------------------------------- Shutting down VM"
nreduce --client $LAST shutdown
sleep 5
echo "-------------------------------- sync"
parsh -h $JOB_DIR/jobnodes sync

echo "-------------------------------- Checking for crash"
check_for_crash

echo "-------------------------------- Showing network stats"
java -cp ~/dev/evaluation util.CollateNetStats ~/logs
