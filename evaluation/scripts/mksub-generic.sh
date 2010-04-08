#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <jobprefix>"
  exit 1
fi

JOBPREFIX=$1
SUB_DIR=$1
QNAME=`hostname -s`


EXPNAME=`echo $JOBPREFIX | sed -e 's/^\(.*\)-\(.\)/\1/'`
MODE=`echo $JOBPREFIX | sed -e 's/^\(.*\)-\(.\)/\2/'`

if [ "$MODE" != o -a "$MODE" != y ]; then
  echo Invalid mode $MODE
  exit 1
fi

if [ ! -e ~/dev/evaluation/scripts/$EXPNAME.params ]; then
  echo $EXPNAME.params does not exist
  exit 1
fi

mkdir -p $SUB_DIR

for ((run = 0; run < 3; run++)); do
  for ((nodes = 1; nodes <= 32; nodes++)); do
    jobname=$JOBPREFIX.r$run.n$nodes
    cat > $SUB_DIR/$jobname.sub <<EOF
#!/bin/sh

#PBS -V

### Job name
#PBS -N $jobname

### Join queuing system output and error files into a single output file
#PBS -j oe

### Don't send email
#PBS -m n

### email address for user
#PBS -M pmk@cs.adelaide.edu.au

### Queue name that job is submitted to
#PBS -q $QNAME

### Request nodes NB THIS IS REQUIRED
#PBS -l nodes=$((nodes+1)):ppn=2,walltime=01:00:00

# This job's working directory
echo Working directory is \$PBS_O_WORKDIR
cd \$PBS_O_WORKDIR
echo Running on host \`hostname\`
echo Time is \`date\`

# Run the executable
export JOB_DIR=~/jobs/$JOBPREFIX/$jobname
mkdir -p \$JOB_DIR
~/dev/evaluation/scripts/generic-$MODE.sh ~/dev/evaluation/scripts/$EXPNAME.params >\$JOB_DIR/output 2>&1
EOF
  done
done
