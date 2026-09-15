#!/bin/bash
# Exercise the same exact FE/BE/runtime-spill inventory with segmented DA.
export LOCAL_QUEUE_S5_ENGINE=segmentedDisk
. ${srcdir:=.}/local-queue-s5-spill.sh
