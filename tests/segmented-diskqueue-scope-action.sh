#!/bin/bash
# segmentedDisk wrapper for the backend-neutral action-queue scope driver.
export QUEUE_TYPE=segmentedDisk QUEUE_SCOPE=action
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
