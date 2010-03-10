#!/bin/bash

queries=`cat usequeries.txt`
resdir=$1

if [ ! -d "$resdir" ]; then
  echo "Please specify results dir"
  exit 1
fi

getavg()
{
  local q=$1
  local cmd=$2

  local all=
  local fails=
  for ((run = 0; run < 5; run++)); do
    local this=`grep RESULT $resdir/query-limits.r$run.$q/output | grep $cmd | awk '{ print $4 }'`
    local fail=`grep RESULT $resdir/query-limits.r$run.$q/output | grep $cmd | awk '{ print $5 }'`
    #grep RESULT $resdir/query-limits.r$run.$q/output | grep $cmd | awk '{ print $5 }'
    all="$all $this"
    fails="$fails $fail"
  done
  local avg=`average $all`
  local realavg=`perl -e "printf('%.3f',$avg/1000.0);"`
  local fsize=`xmlgen -f $realavg | countbytes`
#  echo $q $cmd $avg $fails
  printf "%-6s %-6s %-6s " $realavg $fsize $fail
}


printf "%-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s\n" "Query" "XQ/NT" "XQ/NSz" "XQ/NF"\
       "C/NT" "C/NSz" "C/NF"  "SaxonT" "SaxonS" "SaxonF"
printf "%-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s\n" \
       "-----" "-----" "------" "-----" "-----" "------" "-----" "------" "------" "------"
for q in $queries; do
  printf "%-6s " $q
  getavg $q xqnelc
  getavg $q xqnc
  getavg $q saxon
  printf "\n"
done
