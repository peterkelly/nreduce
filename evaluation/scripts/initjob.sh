#!/bin/bash

if [ -z $JOB_DIR ]; then
  echo JOB_DIR is not set!
  exit 1
fi

if [ -z $PBS_NODEFILE ]; then
  echo PBS_NODEFILE is not set!
  exit 1
fi

uniq $PBS_NODEFILE > $JOB_DIR/jobnodes
uniq $PBS_NODEFILE | sed -e 's/$/:5000/' > $JOB_DIR/jobservers

echo Job nodes
cat $JOB_DIR/jobnodes
echo Job servers
echo $JOB_DIR/jobservers
