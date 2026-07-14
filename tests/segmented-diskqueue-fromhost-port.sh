#!/bin/bash
# Run the asynchronous-ruleset sender-port persistence regression unchanged
# against segmentedDisk.
export QUEUE_TYPE=segmentedDisk
export QUEUE_NEEDS_FILENAME=on
. ${srcdir:=.}/fromhost-port-async-ruleset.sh
