#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <subfilesdir>"
  exit 1
fi

SUB_DIR=$1
QNAME=`hostname -s`

expname=marks-scale-y-fishhalf
nodes=32

for ((run = 0; run < 3; run++)); do
  for ((ms = 1; ms <= 10; ms++)); do
    jobname=$expname.r$run.ms$ms
    if ((ms >= 8)); then
      walltime=00:30:00
    else
      walltime=00:10:00
    fi
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
#PBS -l nodes=$((nodes+1)):ppn=2,walltime=$walltime

# This job's working directory
echo Working directory is \$PBS_O_WORKDIR
cd \$PBS_O_WORKDIR
echo Running on host \`hostname\`
echo Time is \`date\`

# Run the executable
export JOB_DIR=~/jobs/$expname/$jobname
mkdir -p \$JOB_DIR
~/dev/evaluation/scripts/$expname.sh $ms >\$JOB_DIR/output 2>&1
EOF
  done
done
