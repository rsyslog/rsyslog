#!/bin/bash
# Exercise whole-batch FE overflow and return-to-FE routing with a LinkedList BE.
# The shared fixture retains its exact ID, ownership and counter oracles.
export LOCAL_QUEUE_TEST_BE_TYPE=LinkedList
. ${srcdir:=.}/local-queue-imtcp-tiered.sh
