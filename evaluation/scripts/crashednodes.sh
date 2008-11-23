#!/bin/bash

if [ -z $JOB_DIR ]; then
  echo JOB_DIR is not set!
  exit 1
fi

first=1

for host in `cat $JOB_DIR/jobnodes`; do
  if ! grep -q 'Shutdown complete' $JOB_DIR/logs/$host.log; then
    if ((first == 0)); then
      echo -n " "
    fi
    first=0
    echo -n $host
  fi
done
