#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 4)); then
  echo "Usage: $0 <jobsdir> <plotdir> <plotname> <title>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3
TITLE=$4

mkdir -p $PLOT_DIR/plots/$PLOT_NAME
echo P=128.0/129.0 > $PLOT_DIR/plots/$PLOT_NAME/amdahl_p
$SCRIPT_DIR/plotjob.sh "$JOBS_DIR" "$PLOT_DIR" "$PLOT_NAME" "$TITLE"
