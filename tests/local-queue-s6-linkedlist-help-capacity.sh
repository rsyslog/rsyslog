#!/bin/bash
# Exercise bounded LinkedList capacity release by borrowed completion.
# The shared fixture retains its exact ID, ownership and counter oracles.
export LOCAL_QUEUE_TEST_BE_TYPE=LinkedList
. ${srcdir:=.}/local-queue-help-capacity.sh
