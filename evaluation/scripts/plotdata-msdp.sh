#!/bin/bash

SCRIPT_DIR=`dirname $0`

$SCRIPT_DIR/plotdata.sh "$1" "$2" "$3" "$4" '     , 5*(64*(128-128/x)+(x-1)*64)/1024 title "Choreography predicted" with lines ls 4'
