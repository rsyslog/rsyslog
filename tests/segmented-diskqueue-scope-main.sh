#!/bin/bash
# segmentedDisk wrapper for the backend-neutral main-queue scope driver.
export QUEUE_TYPE=segmentedDisk QUEUE_SCOPE=main
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
