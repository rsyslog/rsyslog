#!/bin/bash
# A shared global middle queue intentionally merges producers. Its actual
# workers must nevertheless register local FEs at the next queue. Reuse the
# graph test's exact ID, copied-mutation and positive downstream FE oracles.
export LOCAL_QUEUE_S4_GLOBAL_BOUNDARY=1
. ${srcdir:=.}/local-queue-s4-graph.sh
