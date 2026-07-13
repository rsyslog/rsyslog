#!/bin/bash
# Classic Disk wrapper for the backend-neutral action-queue scope driver.
export QUEUE_TYPE=Disk QUEUE_SCOPE=action
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
