#!/bin/bash
# Reconcile LinkedList FE/BE route and lifetime counters across resettable scrapes.
# The shared fixture retains its exact ID, ownership and counter oracles.
export LOCAL_QUEUE_TEST_BE_TYPE=LinkedList
. ${srcdir:=.}/local-queue-stats-reconciliation.sh
