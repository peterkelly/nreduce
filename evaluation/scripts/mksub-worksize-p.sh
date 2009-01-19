#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <subfilesdir>"
  exit 1
fi

SUB_DIR=$1
QNAME=`hostname -s`

expname=worksize-p

nodes=8

for ((run = 0; run < 5; run++)); do
  for ((x = 32; x <= 1024; x *= 2)); do
    for n in 1 2 3 4 5 6 8 9 11 13 16 19 22 26 32 38 45 53 64 76 90 107 128 152 181 215 256 304 362 430 512 608 724 861 1024 1217 1448 1722 2048 2435 2896 3444 4096; do
      jobname=$expname.x$x.r$run.n$n
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
export JOB_DIR=~/jobs/$expname/$jobname
mkdir -p \$JOB_DIR
~/dev/evaluation/scripts/$expname.sh $x $n >\$JOB_DIR/output 2>&1
EOF
    done
  done
done
