#!/bin/bash
# Recover LinkedList BE and FE holdings across repeated classic DA saves.
# The shared fixture retains its exact ID, ownership and counter oracles.
export LOCAL_QUEUE_TEST_BE_TYPE=LinkedList
. ${srcdir:=.}/local-queue-s5-persistence.sh
