#!/bin/bash

if (($# < 1 )); then
  echo "Usage: $0 <jobdir>"
  exit 1
fi

JOB_DIR=$1

SCRIPT_DIR=`dirname $0`

mkdir $JOB_DIR/times 2>/dev/null
mkdir $JOB_DIR/plot 2>/dev/null

rm -f $JOB_DIR/times/*
rm -f $JOB_DIR/plot/*
$SCRIPT_DIR/gettimes.sh $JOB_DIR $JOB_DIR/times
java -cp $SCRIPT_DIR/.. util.Plot2D $JOB_DIR/plot $JOB_DIR/times

echo "Plot generated in $JOB_DIR/plot"
