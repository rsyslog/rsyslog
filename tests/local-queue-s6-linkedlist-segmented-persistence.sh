#!/bin/bash
# Recover LinkedList BE and FE holdings across repeated segmented DA saves.
# The shared fixture retains its exact ID, ownership and counter oracles.
export LOCAL_QUEUE_TEST_BE_TYPE=LinkedList
export LOCAL_QUEUE_S5_ENGINE=segmentedDisk
. ${srcdir:=.}/local-queue-s5-persistence.sh
