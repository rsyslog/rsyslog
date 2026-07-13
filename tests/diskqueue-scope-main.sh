#!/bin/bash
# Classic Disk wrapper for the backend-neutral main-queue scope driver.
export QUEUE_TYPE=Disk QUEUE_SCOPE=main
. ${srcdir:=.}/testsuites/pure-disk-queue-scope-driver.sh
