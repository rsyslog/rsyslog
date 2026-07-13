#!/bin/bash
# Run the asynchronous-ruleset sender-port persistence regression unchanged
# against segmentedDisk.
export QUEUE_TYPE=segmentedDisk
. ${srcdir:=.}/fromhost-port-async-ruleset.sh
