#!/bin/bash
# A Direct main queue is still a graph lifetime owner: destroying it must stop
# the queued roots before freeing pointers used by later graph shutdown. Reuse
# exact graph delivery/copy checks and the daemon clean-shutdown oracle.
export LOCAL_QUEUE_S4_DIRECT_MAIN=1
. ${srcdir:=.}/local-queue-s4-graph.sh
