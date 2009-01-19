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


if (($# < 3)); then
  echo "Usage: $0 <expdir> <expname> <datafile>"
  exit 1
fi

EXP_DIR=$1
EXP_NAME=$2
DATA_FILENAME=$3

check_job()
{
  local jobdir=$1

  # Check that the job directory exits
  if [ ! -d $jobdir ]; then
    echo $jobdir: Directory does not exist
    exit 1
  fi

  # Check that the job completed
  if [ ! -e $jobdir/done ]; then
    echo $jobdir did not complete successfully
    exit 1
  fi

  # Check that the job exited with status 0
  if ! grep -q 'with status 0' $jobdir/output; then
    echo $jobdir did not exit with status 0
    exit 1
  fi

  # Check that output contains execution time
  if ! grep -q 'Total execution time' $jobdir/output; then
    echo $jobdir output does not contain execution time
    exit 1
  fi
}

rvalues=`cd $EXP_DIR && for i in $EXP_NAME.*; do echo $i; \
         done | sed -e 's/^.*r\(.*\).n.*$/\1/' | sort | uniq`
nvalues=`cd $EXP_DIR && for i in $EXP_NAME.*; do echo $i; \
         done | sed -e 's/^.*n//' | sort -n | uniq`

#echo rvalues $rvalues
#echo nvalues $nvalues

echo -n "Getting times for $EXP_NAME"
echo "# n avg min max" > $DATA_FILENAME
allmin=""
for n in $nvalues; do
  values=""
  for r in $rvalues; do
    job=$EXP_NAME.r$r.n$n
    check_job $EXP_DIR/$job
    seconds=`grep 'Total execution time' $EXP_DIR/$job/output | sed -e 's/^.*: //'`
    values="$values $seconds"
  done
  avg=`average $values`
  min=`min $values`
  max=`max $values`
  allmin="$allmin $avg"
  echo -n .
  echo "$n $avg $min $max" >> $DATA_FILENAME
done
globalmin=`min $allmin`
echo $globalmin > $DATA_FILENAME.globalmin
echo
