#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Run the mixed-graph residual-loss oracle with a generic LinkedList queue.
# The shared fixture checks the same in-flight plus queued ownership count and
# one post-exit report, covering the second memory representation drained by
# the graph finalizer.
export LOCAL_QUEUE_GLOBAL_QUEUE_TYPE=LinkedList
. ${srcdir:=.}/local-queue-global-shutdown-loss-report.sh
