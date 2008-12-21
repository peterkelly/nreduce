
#!/bin/bash

SCRIPT_DIR=`dirname $0`
ELC_DIR=$SCRIPT_DIR/../elc

. $SCRIPT_DIR/common.sh

startservice services.Compute2 1234
startshowload_servicenodes

echo "Startup completed"

echo nreduce $ELC_DIR/pipeline.elc 20 50 `grep -v $HOSTNAME $JOB_DIR/jobnodes`
time nreduce $ELC_DIR/pipeline.elc 20 50 `grep -v $HOSTNAME $JOB_DIR/jobnodes`
echo Program exited with status $?

shutdown
