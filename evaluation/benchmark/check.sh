#!/bin/bash

dir=$1

if [ -z "$dir" ]; then
  echo Please specify directory to check
  exit 1
fi

cd $dir

programs=`for i in *;
            do echo $i;
          done |\
          grep _ |\
          perl -pe 's/^(.+?_.+?)_.*$/$1/' |\
          sort |\
          uniq`

#echo "Checking benchmarks for inconsistent output..."

for prog in $programs; do
#  echo "Test: $prog"
  runs=`for i in ${prog}*; do echo $i; done | grep -v time`
  base=${prog}_elc
  for run in $runs; do
    if ! diff -q $run $base > /dev/null; then
      echo "  Different: $run"
    fi
  done
done

#echo done

