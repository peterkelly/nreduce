EXECUTABLES="$HOME/dev/nreduce/src/nreduce $HOME/dev/tools/loadbal"

export TIMEFORMAT=$'\nTotal execution time: %R'
export VM_RUNNING=0

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
  echo "HOSTNAME is $HOSTNAME"

  if [ -e $JOB_DIR/started ]; then
    fail "Job dir has already been used"
  fi

  mkdir -p $JOB_DIR >/dev/null 2>&1
  mkdir -p $JOB_DIR/logs >/dev/null 2>&1
  touch $JOB_DIR/started

  sort $PBS_NODEFILE | uniq > $JOB_DIR/jobnodes
  cat $JOB_DIR/jobnodes | grep -v $HOSTNAME > $JOB_DIR/computenodes
  sort $JOB_DIR/computenodes | sed -e 's/$/:5000/' > $JOB_DIR/computeservers

  INITIAL=`head -1 $JOB_DIR/computenodes`
  echo "INITIAL is $INITIAL"
  grep -v $INITIAL $JOB_DIR/computenodes > $JOB_DIR/vm2nodes

  echo "Nodes:" `cat $JOB_DIR/jobnodes`

  echo -n "Killing existing processes... "
  parsh -h $JOB_DIR/jobnodes 'killall -9 java nreduce loadbal showload svc_compute compute' >/dev/null 2>&1
  echo "ok"

  echo -n "Clearing log dir... "
  mkdir -p $JOB_DIR/logs/ 2>/dev/null
  rm -f $JOB_DIR/logs/* 2>/dev/null
  echo "ok"

  echo -n "Listing all processes... "
  mkdir $JOB_DIR/processes
  parsh -h $JOB_DIR/jobnodes $SCRIPT_DIR/listprocs.sh $JOB_DIR
  echo "ok"
}

startservice()
{
  local service=$1
  local port=$2

  echo -n "Starting Java service $service on port $port... "
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

startcservice()
{
  local service=$1
  local port=$2

  echo -n "Starting C service $service on port $port... "
  rm -f $JOB_DIR/logs/service.*
  parsh -h $JOB_DIR/jobnodes \
        "cd ~ && $SCRIPT_DIR/service.sh $JOB_DIR $service $port > $JOB_DIR/logs/service.\$HOSTNAME 2>&1"\
        >$JOB_DIR/logs/parsh_service.out &
  local node
  for node in `cat $JOB_DIR/jobnodes`; do
    ~/dev/tools/waitconn $node $port 30
  done
  echo "ok"
}

startvm()
{
  echo -n "Starting virtual machine (excluding client node)... "

  # Start the initial node
  ssh $INITIAL "$SCRIPT_DIR/initial.sh $JOB_DIR $INITIAL $LOG_LEVEL" &
  echo "Initial node started: $INITIAL"
  ~/dev/tools/waitconn $INITIAL 6879 30

  # Start the other nodes
  parsh -h $JOB_DIR/vm2nodes \
        "$SCRIPT_DIR/slave.sh $JOB_DIR $INITIAL $LOG_LEVEL" \
        >$JOB_DIR/logs/parsh.out &
  echo "Other nodes started: `cat $JOB_DIR/vm2nodes`"
  wait_vm_startup
  echo "ok"

  VM_RUNNING=1
}

startloadbal()
{
  echo -n "Starting load balancer... "
  (~/dev/tools/loadbal -p 32768-49151 -l - 5001 $JOB_DIR/computeservers >$JOB_DIR/loadbal.log 2>&1 &)
  ~/dev/tools/waitconn $HOSTNAME 5001 30
  echo "ok"
}

startshowload()
{
  echo -n "Starting showload (compute nodes only)... "
  (~/dev/tools/showload -q -l $JOB_DIR/showload.log $JOB_DIR/computenodes >/dev/null 2>&1 &)
  echo "ok"
}

wait_vm_startup()
{
  # Wait until all nodes have joined
  local nodecount=0
  local expected=`cat $JOB_DIR/computenodes | wc -l`
  local count=0
  while true; do
    if ((count >= 15)); then
      echo "chord stabilization failed after $count seconds"
      exit 1
    fi
    ~/dev/nreduce/src/nreduce --client $INITIAL findall > $JOB_DIR/found_nodes

    echo Found nodes:
    cat $JOB_DIR/found_nodes

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
  for host in `cat $JOB_DIR/computenodes`; do
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

shutdown()
{
  check_env

  echo -n "Stopping showload... "
  killall -9 showload >/dev/null 2>&1
  echo "ok"

  echo -n "Stopping load balancer... "
  killall -9 loadbal >/dev/null 2>&1
  echo "ok"

  echo -n "Stopping services... "
  parsh -h $JOB_DIR/jobnodes 'killall -9 java svc_compute compute' >/dev/null 2>&1
  echo "ok"

  if [ $VM_RUNNING -ne 0 ]; then
    echo -n "Requesting shutdown of virtual machine... "
    nreduce --client $INITIAL shutdown >/dev/null 2>&1
    sleep 3
    echo "ok"
  fi

  echo -n "Performing filesystem sync... "
  parsh -h $JOB_DIR/jobnodes sync >/dev/null
  echo "ok"

  if [ $VM_RUNNING -ne 0 ]; then
    echo -n "Checking if all nodes shut down successfully... "
    check_for_crash
    echo "ok"

    echo -n "Killing any lingering VM nodes... "
    parsh -h $JOB_DIR/jobnodes 'killall -9 nreduce' >/dev/null 2>&1
    echo "ok"

    echo -n "Collating netstats to $JOB_DIR/logs/netstats... "
    java -cp ~/dev/evaluation util.CollateNetStats $JOB_DIR/logs > $JOB_DIR/logs/netstats
    echo "ok"
  fi

  echo -n "Generating plot of machine usage... "
  $SCRIPT_DIR/plotload.sh $JOB_DIR/showload.log $JOB_DIR/usage.eps
  $SCRIPT_DIR/plotload_total.sh $JOB_DIR/showload.log $JOB_DIR/usage_total.eps
  echo "ok"

  touch $JOB_DIR/done
}

init
