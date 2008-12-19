EXECUTABLES="$HOME/dev/nreduce/src/nreduce $HOME/dev/tools/loadbal"

export TIMEFORMAT="Total execution time: %R"

fail()
{
  if (($# >= 1)); then
    echo "Failure: $@"
  else
    echo "Failure: unknown reason"
  fi
  exit 1
}

check_env()
{
  if [ -z $JOB_DIR ]; then
    fail "\$JOB_DIR is not set!"
  fi
  if [ -z $PBS_NODEFILE ]; then
    fail "\$PBS_NODEFILE is not set!"
  fi
  if [ -z $SCRIPT_DIR ]; then
    fail "\$SCRIPT_DIR is not set!"
  fi

  local exec
  for exec in $EXECUTABLES; do
    if [ ! -f $exec ]; then
      fail "Required file $exec is missing"
    fi
  done
}

init()
{
  check_env

  echo "SCRIPT_DIR is $SCRIPT_DIR"
  echo "JOB_DIR is $JOB_DIR"
  echo "PBS_NODEFILE is $PBS_NODEFILE"

  if [ -e $JOB_DIR/started ]; then
    fail "Job dir has already been used"
  fi

  mkdir -p $JOB_DIR >/dev/null 2>&1
  mkdir -p $JOB_DIR/logs >/dev/null 2>&1
  touch $JOB_DIR/started

  `sort $PBS_NODEFILE | uniq > $JOB_DIR/jobnodes`
  `sort $PBS_NODEFILE | uniq | sed -e 's/$/:1234/' > $JOB_DIR/jobservers`
  `cat $JOB_DIR/jobnodes | grep -v $HOSTNAME > $JOB_DIR/othernodes`
  `cat $JOB_DIR/jobservers | grep -v $HOSTNAME > $JOB_DIR/otherservers`

  echo "Nodes:" `cat $JOB_DIR/jobnodes`
}

startservice()
{
  local service=$1
  local port=$2

  echo -n "Starting service $service on port $port... "
  rm -f $JOB_DIR/logs/service.*
  parsh -h $JOB_DIR/jobnodes \
        "cd ~/dev/evaluation && java $service $port > $JOB_DIR/logs/service.\$HOSTNAME 2>&1"\
        >$JOB_DIR/logs/parsh_service.out &
  local node
  for node in `cat $JOB_DIR/jobnodes`; do
    ~/dev/tools/waitconn $node $port 30
  done
  echo "ok"
}

startvm()
{
  echo -n "Starting virtual machine... "

  # Start the initial node
  ~/dev/nreduce/src/nreduce -w >$JOB_DIR/logs/$HOSTNAME.log 2>&1 &
  echo "Initial node started: $HOSTNAME"
  ~/dev/tools/waitconn $HOSTNAME 2000 30

  # Start the other nodes
  parsh -h $JOB_DIR/othernodes \
        "$SCRIPT_DIR/slave.sh $JOB_DIR $HOSTNAME $LOG_LEVEL" \
        >$JOB_DIR/logs/parsh.out &
  echo "Other nodes started: `cat $JOB_DIR/othernodes`"
  wait_vm_startup
  echo "ok"
}

startloadbal()
{
  echo -n "Starting load balancer... "
  (~/dev/tools/loadbal -l $JOB_DIR/loadbal.log 1235 $JOB_DIR/otherservers >/dev/null 2>&1 &)
  echo "ok"
}

startshowload()
{
  echo -n "Starting showload... "
  (~/dev/tools/showload -q -l $JOB_DIR/showload.log $JOB_DIR/jobnodes >/dev/null 2>&1 &)
  echo "ok"
}

wait_vm_startup()
{
  # Wait until all nodes have joined
  local nodecount=0
  local expected=`cat $JOB_DIR/jobnodes | wc -l`
  local count=0
  while true; do
    if ((count >= 15)); then
      echo "chord stabilization failed after $count seconds"
      exit 1
    fi
    ~/dev/nreduce/src/nreduce --client $HOSTNAME findall > $JOB_DIR/found_nodes
    nodecount=`cat $JOB_DIR/found_nodes | wc -l`
    if ((nodecount == expected)); then
      break
    fi
    ((count++))
    sleep 1
  done
}

check_for_crash()
{
  local crashed=""
  local host
  for host in `cat $JOB_DIR/jobnodes`; do
    if ! grep -q 'Shutdown complete' $JOB_DIR/logs/$host.log; then
      if [ -z "$crashed" ]; then
        crashed="$host"
      else
        crashed="$crashed $host"
      fi
    fi
  done

  if [ ! -z "$crashed" ]; then
    echo "Crashed nodes: $crashed"
    exit 1
  fi
}

startup()
{
  init
  if (($# < 2)); then
    echo "startup_exp: service or port missing"
    return 1
  fi

  local service=$1
  local port=$2

  echo -n "Killing existing processes... "
  parsh -h $JOB_DIR/jobnodes 'killall -9 java nreduce' >/dev/null 2>&1
  sleep 3
  echo "ok"

  echo -n "Clearing log dir... "
  mkdir -p $JOB_DIR/logs/ 2>/dev/null
  rm -f $JOB_DIR/logs/* 2>/dev/null
  echo "ok"

  startloadbal
  startshowload
  startservice $service $port
  startvm

  echo "Startup completed"
  return 0
}

shutdown()
{
  check_env

  echo -n "Stopping load balancer... "
  killall -9 loadbal >/dev/null 2>&1
  echo "ok"

  echo -n "Stopping showload... "
  killall -9 showload >/dev/null 2>&1
  echo "ok"

  echo -n "Stopping services... "
  parsh -h $JOB_DIR/jobnodes 'killall -9 java'
  echo "ok"

  echo -n "Requesting shutdown of virtual machine... "
  nreduce --client $HOSTNAME shutdown >/dev/null 2>&1
  sleep 3
  echo "ok"

  echo -n "Performing filesystem sync... "
  parsh -h $JOB_DIR/jobnodes sync >/dev/null
  echo "ok"

  echo -n "Checking if all nodes shut down successfully... "
  check_for_crash
  echo "ok"

  echo -n "Collating netstats to $JOB_DIR/logs/netstats... "
  java -cp ~/dev/evaluation util.CollateNetStats $JOB_DIR/logs > $JOB_DIR/logs/netstats
  echo "ok"

  echo -n "Generating plot of machine usage... "
  $SCRIPT_DIR/plotload.sh $JOB_DIR/showload.log $JOB_DIR/usage.ps
  echo "ok"

  touch $JOB_DIR/done
}
