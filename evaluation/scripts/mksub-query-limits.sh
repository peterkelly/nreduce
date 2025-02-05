#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <subfilesdir>"
  exit 1
fi

SUB_DIR=$1
QNAME=`hostname -s`

queries=`cat ~/dev/evaluation/xquery/benchmark/usequeries.txt`

for q in $queries; do
  for ((run = 0; run < 5; run++)); do
    jobname=query-limits.r$run.$q
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
export JOB_DIR=~/jobs/query-limits/$jobname
mkdir -p \$JOB_DIR
cd \$JOB_DIR
~/dev/evaluation/xquery/benchmark/query-limit.sh ~/dev/xquery/query $q xqnelc >\$JOB_DIR/output 2>&1
~/dev/evaluation/xquery/benchmark/query-limit.sh ~/dev/xquery/queryc $q xqncp >>\$JOB_DIR/output 2>&1
~/dev/evaluation/xquery/benchmark/query-limit.sh saxonq $q saxon >>\$JOB_DIR/output 2>&1
echo DONE
EOF
  done
done
