#!/bin/bash
# Keep each third of 198 ingress IDs, then reuse repeated segmentedDisk persistence.
# Exact 66 retained IDs after both restarts prove transfers never sample again.
export LOCAL_QUEUE_S6_SAMPLING=3
export LOCAL_QUEUE_S5_ENGINE=segmentedDisk
. ${srcdir:=.}/local-queue-s5-persistence.sh
