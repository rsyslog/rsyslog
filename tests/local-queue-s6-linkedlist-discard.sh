#!/bin/bash
# Apply the same FE-only logical-pressure and resource-bound oracle to LinkedList.
export LOCAL_QUEUE_TEST_BE_TYPE=LinkedList
. ${srcdir:=.}/local-queue-s6-discard.sh
