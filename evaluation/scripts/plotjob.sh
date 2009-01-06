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

OUTPUT_DIR=$PLOT_DIR/plots/$PLOT_NAME

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$PLOT_NAME

# Extract + collate execution times from all runs
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-o $PLOT_NAME-o $OUTPUT_DIR/$PLOT_NAME-o.dat
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-y $PLOT_NAME-y $OUTPUT_DIR/$PLOT_NAME-y.dat

# Plot execution time and speedup
cd $OUTPUT_DIR
$SCRIPT_DIR/plotjob-time.sh $PLOT_NAME "$TITLE"
$SCRIPT_DIR/plotjob-speedup.sh $PLOT_NAME "$TITLE"
cp -f *.eps ../../eps
