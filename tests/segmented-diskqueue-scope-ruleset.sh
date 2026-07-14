#!/bin/bash
# segmentedDisk wrapper for the backend-neutral ruleset-queue scope driver.
export QUEUE_TYPE=segmentedDisk QUEUE_SCOPE=ruleset
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
