#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 5)); then
  echo "Usage: $0 <jobsdir> <plotdir> <name> <smalllabel> <biglabel>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
NAME=$3
SMALL_LABEL=$4
BIG_LABEL=$5

mkdir -p $PLOT_DIR/plots/$NAME
echo P=128.0/129.0 > $PLOT_DIR/plots/$NAME/amdahl_p
$SCRIPT_DIR/plotjob2.sh "$JOBS_DIR" "$PLOT_DIR" "$NAME" "$SMALL_LABEL" "$BIG_LABEL"
