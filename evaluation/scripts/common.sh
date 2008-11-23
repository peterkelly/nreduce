check_for_crash()
{
  local CRASHED=`~/dev/evaluation/scripts/crashednodes.sh`

  if [ ! -z "$CRASHED" ]; then
    echo "Nodes $CRASHED crashed"
    exit 1
  fi

  echo "All nodes shut down successfully"
}
