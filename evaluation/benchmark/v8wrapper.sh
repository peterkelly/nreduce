#!/bin/bash
program=$1
arg=$2

tempfile=`mktemp`

echo "arguments = [$arg];" > $tempfile
cat $program >> $tempfile
v8shell $tempfile
rm -f $tempfile
