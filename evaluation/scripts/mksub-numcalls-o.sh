#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <subfilesdir>"
  exit 1
fi

SUB_DIR=$1
QNAME=`hostname -s`

expname=numcalls-o
nodes=1

for ms in 0; do
  for ((run = 0; run < 10; run++)); do
#    for ((numcalls = 1; numcalls <= 2097152; numcalls *= 2)); do
    for ((numcalls = 1; numcalls <= 4194304; numcalls *= 2)); do
      jobname=$expname.ms$ms.r$run.n$numcalls
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
#PBS -l nodes=$((nodes)):ppn=2,walltime=01:00:00

# This job's working directory
echo Working directory is \$PBS_O_WORKDIR
cd \$PBS_O_WORKDIR
echo Running on host \`hostname\`
echo Time is \`date\`

# Run the executable
export JOB_DIR=~/jobs/$expname/$jobname
mkdir -p \$JOB_DIR
~/dev/evaluation/scripts/$expname.sh $numcalls $ms >\$JOB_DIR/output 2>&1
EOF
    done
  done
done
