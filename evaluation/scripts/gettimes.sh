#!/bin/bash

# Usage: gettimes.sh <expdir> <outdir>
#
# Converts the output of jobs in the new format to the time listing used by
# Plot2D. This consists of going through a directory containing a list of
# job directories, checking that each completed, extracting the running
# time from $JOB_DIR/output, and writing it to a file time.rX.nY, where X
# is the run number and Y is the number of nodes.

# The individual job directories are expected to be in the form EXPNAME.rX.nY,
# where EXPNAME is the name of the experiment (e.g. basicmap).


if (($# < 2)); then
  echo "Usage: $0 <expdir> <outdir>"
  exit 1
fi

EXP_DIR=$1
OUT_DIR=$2

echo $EXP_DIR | sed -e 's/\///' > $OUT_DIR/title

# Go through each job directory
for job in $(cd $EXP_DIR; echo *); do

  # Check format of job name
  if ! echo $job | grep -qE '^.*\.r[0-9]+\.n[0-9]+'; then
    echo Skipping $EXP_DIR/$job
    continue
  fi

  # Check for successful completion
  if [ ! -e $EXP_DIR/$job/done ]; then
    echo Job $EXP_DIR/$job did not complete successfully
    exit 1
  fi
  if ! grep -q END $EXP_DIR/$job/output; then
    echo Job output for $EXP_DIR/$job does not contain END token
    exit 1
  fi
  if ! grep -qF 'Total execution time' $EXP_DIR/$job/output; then
    echo Job output for $EXP_DIR/$job does not execution time
    exit 1
  fi

  # Extract time from output
  run=`echo $job | sed -E -e 's/^.*\.r([0-9]+)\.n([0-9]+)/\1/'`
  nodes=`echo $job | sed -E -e 's/^.*\.r([0-9]+)\.n([0-9]+)/\2/'`
  seconds=`grep 'Total execution time' $EXP_DIR/$job/output | sed -e 's/^.*: //'`
  echo run=$run nodes=$nodes seconds=$seconds
  echo "$seconds 0.0 0.0" > $OUT_DIR/time.r$run.n$nodes
done
