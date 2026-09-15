#!/bin/bash
# Run the shared unavailable-store oracle with segmented retention/retry policy.
export LOCAL_QUEUE_S5_ENGINE=segmentedDisk
. ${srcdir:=.}/local-queue-s5-store-failure.sh
