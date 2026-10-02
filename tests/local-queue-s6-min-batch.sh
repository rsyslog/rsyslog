#!/bin/bash
# An isolated singleton must complete through the minimum timeout. Afterwards
# F=3 clamps requested minimum5/dequeue8: queued three must form exactly one
# batch. Per-FE batch totals and exact IDs are the oracle, not elapsed time.
export LOCAL_QUEUE_S6_MIN_BATCH=1
. ${srcdir:=.}/local-queue-imtcp-partial-batch.sh
