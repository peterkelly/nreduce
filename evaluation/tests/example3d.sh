#!/bin/bash

INITIAL=`hostname`
TIMEFORMAT="%R %U %S"
OUTDIR=results/example3d

if [ -e $OUTDIR ]; then
  echo "$OUTDIR already exists; please remove first"
  exit 1
fi

mkdir $OUTDIR

echo 'Example 3D' > $OUTDIR/title

runtest()
{
  echo run $run nodes $nodes
  suffix=r$run.n$nodes.v$v

  runservice services.Compute
  runvm

  (time \
    nreduce --client $INITIAL run $nodes elc/computev.elc $v \
    >$OUTDIR/out.$suffix 2>$OUTDIR/err.$suffix) 2>$OUTDIR/time.$suffix

  killvm
  killjava
}

for ((run = 0; run < 1; run++)); do
  for ((v = 5; v <= 20; v += 5)); do
    nodes=1
    runtest
    for ((nodes = 2; nodes <= 8; nodes += 2)); do
      runtest
    done
  done
done
