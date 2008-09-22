#!/bin/bash

run=1
while ((run<=1)); do
  i=50
  while ((i<1000)); do
  #  echo i $i

    time nreduce requests.elc 100 localhost 80 "/cgi-bin/data?size=1M&speed=${i}k" > time.$run.$i

    ((i += 50))
  done
  ((run++))
done
