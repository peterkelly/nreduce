#!/bin/bash
queries=`cat usequeries.txt`
factor=.002
differences=
for q in $queries; do
  echo Query: $q
  xmlgen -f $factor | saxonq $q.xq /dev/stdin > output/$q-saxon.out
  if [ $? ]; then
    echo Saxon: ok
  else
    echo Saxon: failed
  fi
  xmlgen -f $factor | ~/dev/xquery/query $q.xq /dev/stdin > output/$q-nreduce.out
  if [ $? ]; then
    echo NReduce: ok
  else
    echo NReduce: failed
  fi
  if diff -q output/$q-saxon.out output/$q-nreduce.out; then
    echo Results are the same
  else
    echo Results are different
    differences="$differences $q"
  fi
  echo
done
echo Differences: $differences
