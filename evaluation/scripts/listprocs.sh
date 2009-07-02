#!/bin/bash

if (($# < 1)); then
  echo "Usage: $0 <jobdir>"
  exit 1
fi

JOB_DIR=$1

ps waux > $JOB_DIR/processes/`hostname`.procs
