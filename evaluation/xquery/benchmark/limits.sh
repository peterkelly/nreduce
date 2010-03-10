#!/bin/bash
queries=`cat usequeries.txt`

ABORT()
{
  echo "Aborting"
  exit 1
}

trap 'ABORT' 2

for q in $queries; do
  ./query-limit.sh ~/dev/xquery/query $q xqn
done
for q in $queries; do
  ./query-limit.sh saxonq $q saxon
done
