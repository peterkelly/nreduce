#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 3)); then
  echo "Usage: $0 <jobsdir> <plotdir> <plotname>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3

OUTPUT_DIR=$PLOT_DIR/plots/$PLOT_NAME

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$PLOT_NAME

# Extract + collate execution times from all runs
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-o $PLOT_NAME-o $OUTPUT_DIR/$PLOT_NAME-o.dat
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$PLOT_NAME-y $PLOT_NAME-y $OUTPUT_DIR/$PLOT_NAME-y.dat

# Plot execution time and speedup
cd $OUTPUT_DIR
$SCRIPT_DIR/plotjob-time.sh $PLOT_NAME
$SCRIPT_DIR/plotjob-speedup.sh $PLOT_NAME
cp -f *.eps ../../eps
