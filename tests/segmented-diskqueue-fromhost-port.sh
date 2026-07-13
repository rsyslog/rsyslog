#!/bin/bash
# Run the asynchronous-ruleset sender-port persistence regression unchanged
# against segmentedDisk.
export QUEUE_TYPE=segmentedDisk
export QUEUE_EXTRA='queue.filename="asyncq" queue.spoolDirectory="'${RSYSLOG_DYNNAME}'.spool"'
. ${srcdir:=.}/fromhost-port-async-ruleset.sh
