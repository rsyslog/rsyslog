#!/bin/bash
# Run the shared downstream DA shutdown ownership oracle with segmented storage.
export LOCAL_QUEUE_S5_ENGINE=segmentedDisk
. ${srcdir:=.}/local-queue-s5-graph-shutdown.sh
