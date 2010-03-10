#!/bin/bash

BENCHMARK_DIR=`dirname $0`

export OPT_MAXHEAP=512
export LOG_LEVEL=info
MAX_MS=30
TIMEFORMAT="%0R"

runtest()
{
  local cmd=$1
  local q=$2
  local f=$3

  realf=`perl -e "print $f/1000"`;
  time (xmlgen -f $realf | $cmd $BENCHMARK_DIR/$q.xq /dev/stdin >/dev/null 2>${name}.$q.err.tmp) 2>time
  if [ $? -ne 0 ]; then
    r=$MAX_MS
    reason=mem
  else
    r=`cat time`
    reason=time
  fi
  rm -f time
}


cmd=$1
q=$2
name=$3

f=1

echo QUERY $q $cmd

# Stage 1
minf=0
failreason=
while true; do
  runtest $cmd $q $f
  if [ $r -lt $MAX_MS ]; then
    minf=$f
    ((f=$f*2))
    mv -f ${name}.$q.err.tmp ${name}.$q.err
    echo "PROGRESS $r seconds: ok; range is $minf-?"
  else
    failreason=$reason
    if [ $reason = time ]; then
      echo "PROGRESS past limit ($r second); range is $minf-$f"
    else
      echo "PROGRESS past limit (memory exhausted); range is $minf-$f"
    fi
    break
  fi
done
maxf=$f

# Stage 2
prevf=0
for ((step = 0; step < 5; step++)); do
  ((f=($minf+$maxf)/2))
  if ((minf == maxf-1)); then
    break;
  fi
  runtest $cmd $q $f
  if [ $r -lt $MAX_MS ]; then
    minf=$f
    mv -f ${name}.$q.err.tmp ${name}.$q.err
    echo "PROGRESS range is $minf-$maxf ($r seconds)"
  else
    maxf=$f
    failreason=$reason
    if [ $reason = time ]; then
      echo "PROGRESS range is $minf-$maxf ($r seconds)"
    else
      echo "PROGRESS range is $minf-$maxf (memory exhausted)"
    fi
  fi
done

rm -f ${name}.$q.err.tmp

echo "RESULT $q $name $minf $failreason"
