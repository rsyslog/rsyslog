#!/bin/bash
# Exercise the shared repeated-save oracle with segmented DA, changed worker
# counts, and local/global/local readers of the same logical spool.
export LOCAL_QUEUE_S5_ENGINE=segmentedDisk
. ${srcdir:=.}/local-queue-s5-persistence.sh
