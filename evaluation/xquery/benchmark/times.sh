#!/bin/bash
queries=`cat usequeries`
xmark=~/xmark/med


ABORT()
{
  echo "Aborting"
  exit 1
}

trap 'ABORT' 2

MAX_MEM=1048576
MAX_CPU=600



#Factor    Saxon     NReduce

printf "%-8s %-8s %-8s %-8s\n" "Factor" "Query" "Saxon" "NReduce"
printf "%-8s %-8s %-8s %-8s\n" "------" "-----" "-----" "-------"


factors=`cd $xmark && for i in 0.*.xml; do echo $i; done | sed -e 's/\.xml//' | sort -n`
TIMEFORMAT="%R"
for f in $factors; do
  xmlfile=$xmark/$f.xml
  for q in $queries; do
    time (ulimit -v $MAX_MEM
          ulimit -t $MAX_CPU
          saxonq benchmark/$q.xq $xmlfile >/dev/null 2>&1) 2>time
    saxonrc=$?
    if [ $saxonrc -eq 0 ]; then
      saxontime=`cat time`
    else
      saxontime=-
    fi
    time (ulimit -v $MAX_MEM
          ulimit -t $MAX_CPU
          ~/dev/xquery/query benchmark/$q.xq $xmlfile >/dev/null 2>&1) 2>time
    nreducerc=$?
    if [ $nreducerc -eq 0 ]; then
      nreducetime=`cat time`
    else
      nreducetime=-
    fi
    printf "%-8s %-8s %-8s %-8s\n" $f $q $saxontime $nreducetime
  done
done
