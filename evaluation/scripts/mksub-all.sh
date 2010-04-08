#!/bin/bash

SCRIPT_DIR=`dirname $0`

$SCRIPT_DIR/mksub-generic.sh dataparallel-big-o
$SCRIPT_DIR/mksub-generic.sh dataparallel-big-y
$SCRIPT_DIR/mksub-generic.sh dataparallel-small-o
$SCRIPT_DIR/mksub-generic.sh dataparallel-small-y

$SCRIPT_DIR/mksub-generic.sh nested-big-o
$SCRIPT_DIR/mksub-generic.sh nested-big-y
$SCRIPT_DIR/mksub-generic.sh nested-small-o
$SCRIPT_DIR/mksub-generic.sh nested-small-y

$SCRIPT_DIR/mksub-generic.sh divconq-big-o
$SCRIPT_DIR/mksub-generic.sh divconq-big-y
$SCRIPT_DIR/mksub-generic.sh divconq-small-o
$SCRIPT_DIR/mksub-generic.sh divconq-small-y

$SCRIPT_DIR/mksub-generic.sh pp-big-o
$SCRIPT_DIR/mksub-generic.sh pp-big-y
$SCRIPT_DIR/mksub-generic.sh pp-small-o
$SCRIPT_DIR/mksub-generic.sh pp-small-y

$SCRIPT_DIR/mksub-generic.sh msdp-big-o
$SCRIPT_DIR/mksub-generic.sh msdp-big-y
$SCRIPT_DIR/mksub-generic.sh msdp-small-o
$SCRIPT_DIR/mksub-generic.sh msdp-small-y
