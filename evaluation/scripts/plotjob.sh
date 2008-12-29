#!/bin/bash

SCRIPT_DIR=`dirname $0`

if (($# < 3)); then
  echo "Usage: $0 <jobdir> <plotdir> <plotname> <jobnames...>"
  exit 1
fi

JOBS_DIR=$1
PLOT_DIR=$2
PLOT_NAME=$3
shift 3
JOB_NAMES=$@

# Check existence of directories
if [ ! -d $JOBS_DIR ]; then
  echo $JOBS_DIR: no such directory
  exit 1
fi

for NAME in $JOB_NAMES; do
  if [ ! -d $JOBS_DIR/$NAME ]; then
    echo $JOBS_DIR/$NAME: no such directory
    exit 1
  fi
done

# Set up directories
mkdir -p $PLOT_DIR/eps
mkdir -p $PLOT_DIR/plots/$PLOT_NAME
for NAME in $JOB_NAMES; do
  rm -rf $PLOT_DIR/times/$NAME
  mkdir -p $PLOT_DIR/times/$NAME
done

# Extract execution times from job logs
for NAME in $JOB_NAMES; do
  $SCRIPT_DIR/gettimes.sh $JOBS_DIR/$NAME $PLOT_DIR/times/$NAME
  echo $NAME > $PLOT_DIR/times/$NAME/title
done


# Generate plots for time, speedup, and efficiency

PLOT_ARGS=
for NAME in $JOB_NAMES; do
  PLOT_ARGS="$PLOT_ARGS $PLOT_DIR/times/$NAME"
done


java -cp $SCRIPT_DIR/.. util.Plot2D $PLOT_DIR/plots/$PLOT_NAME $PLOT_ARGS
echo Generated plots for job $PLOT_NAME in $PLOT_DIR/plots/$PLOT_NAME

# Copy the plots for this job into the common plots directory, prefixed by the job name
cp -f $PLOT_DIR/plots/$PLOT_NAME/time.eps $PLOT_DIR/eps/$PLOT_NAME-time.eps
cp -f $PLOT_DIR/plots/$PLOT_NAME/speedup.eps $PLOT_DIR/eps/$PLOT_NAME-speedup.eps
cp -f $PLOT_DIR/plots/$PLOT_NAME/efficiency.eps $PLOT_DIR/eps/$PLOT_NAME-efficiency.eps
echo Copied plots to $PLOT_DIR/eps
