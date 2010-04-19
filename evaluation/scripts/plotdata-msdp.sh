#!/bin/bash

SCRIPT_DIR=`dirname $0`

$SCRIPT_DIR/plotdata.sh "$1" "$2" "$3" '     , 5*(256*(128-128/x)+(x-1)*256)/1024 title "Choreography predicted" with lines ls 4'
