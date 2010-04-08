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

OUTPUT_DIR=$PLOT_DIR/plots/$NAME

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$NAME

# Extract + collate execution times from all runs
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$NAME-small-o $NAME-small-o $OUTPUT_DIR/$NAME-small-o.dat
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$NAME-small-y $NAME-small-y $OUTPUT_DIR/$NAME-small-y.dat

$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$NAME-big-o $NAME-big-o $OUTPUT_DIR/$NAME-big-o.dat
$SCRIPT_DIR/gettimes.sh $JOBS_DIR/$NAME-big-y $NAME-big-y $OUTPUT_DIR/$NAME-big-y.dat

# Plot execution time and speedup
cd $OUTPUT_DIR
$SCRIPT_DIR/plotjob-time.sh $NAME-big
$SCRIPT_DIR/plotjob-time.sh $NAME-small
$SCRIPT_DIR/plotjob2-speedup.sh "$NAME" "$SMALL_LABEL" "$BIG_LABEL"
cp -f *.eps ../../eps
