#!/bin/bash

dd if=/dev/zero of=testfile bs=1048576 count=100

echo
echo Running experiment with echo1
echo
measure echo1.mes | tee results1

echo
echo Running experiment with echo2
echo
measure echo2.mes | tee results2

rm -f testfile

gnuplot time.plot
gnuplot cs.plot
