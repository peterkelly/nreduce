#!/bin/bash

echo
echo Running experiment with 1b messages
echo
measure 1b.mes | tee results1b

echo
echo Running experiment with 1k messages
echo
measure 1k.mes | tee results1k

echo
echo Running experiment with 10k messages
echo
measure 10k.mes | tee results10k

echo
echo Running experiment with 100k messages
echo
measure 100k.mes | tee results100k

gnuplot time.plot
