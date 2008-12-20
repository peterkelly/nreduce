
#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startup services.Compute 1234

time nreduce $ELC_DIR/basicmap.elc $HOSTNAME 1235 100 100
echo Program exited with status $?

shutdown
