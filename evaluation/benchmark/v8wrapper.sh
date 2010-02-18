#!/bin/bash
program=$1
arg=$2

echo "arguments = [$arg];" > temp.js
cat $program >> temp.js
v8shell temp.js
rm -f temp.js
