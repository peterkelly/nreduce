#!/bin/bash

runcmd()
{
  local CMD=$1
  local LANGUAGE=$2

  local OUTFILE=${PROGRAM}_${N}_${LANGUAGE}
  local ERRFILE=${PROGRAM}_${N}_${LANGUAGE}_err
  local TIMEFILE=${PROGRAM}_${N}_${LANGUAGE}_time

  echo "$CMD $N"

  (time $CMD $N >$TEMPDIR/$OUTFILE 2>$TEMPDIR/$ERRFILE) 2>$OUTDIR/$TIMEFILE
  mv -f $TEMPDIR/$OUTFILE $OUTDIR/$OUTFILE
  mv -f $TEMPDIR/$ERRFILE $OUTDIR/$ERRFILE

  if [ -z "`cat $OUTDIR/$ERRFILE`" ]; then
    rm -f $OUTDIR/$ERRFILE
  fi

}

PROGRAM=$1
N=$2
OUTDIR=$3

if [ -z "$PROGRAM" ]; then
  echo No program specified
  exit 1
fi

if [ -z "$N" ]; then
  echo No N specified
  exit 1
fi

if [ ! -d "$OUTDIR" ]; then
  echo Output dir \"$OUTDIR\" does not exist
  exit 1
fi

#echo PROGRAM $PROGRAM
#echo N $N

TEMPDIR=`mktemp -d /tmp/run.XXXXXX`
#echo "Using tempdir $TEMPDIR"

TIMEFORMAT="%R %U %S"

if [ ${PROGRAM} = "nsieve" ]; then
  runcmd "nreduce -v lazy ${PROGRAM}.elc" "elc"
else
  runcmd "nreduce ${PROGRAM}.elc" "elc"
fi

runcmd "./${PROGRAM}" "c"
if [ -e "${PROGRAM}2" ]; then
  runcmd "./${PROGRAM}2" "c2"
fi

runcmd "java -Xmx256m ${PROGRAM}" "java"
if [ -e "${PROGRAM}2.class" ]; then
  runcmd "java -Xmx256m ${PROGRAM}2" "java2"
fi

runcmd "./${PROGRAM}.pl" "perl"
if [ -e "${PROGRAM}2.pl" ]; then
  runcmd "./${PROGRAM}2.pl" "perl2"
fi

runcmd "./${PROGRAM}.py" "python"
if [ -e "${PROGRAM}2.py" ]; then
  runcmd "./${PROGRAM}2.py" "python2"
fi

runcmd "./v8wrapper.sh ${PROGRAM}.js" "js"
if [ -e "${PROGRAM}2.js" ]; then
  runcmd "./v8wrapper.sh ${PROGRAM}2.js" "js2"
fi

runcmd "./hs${PROGRAM}" "haskell"

rmdir "$TEMPDIR"
