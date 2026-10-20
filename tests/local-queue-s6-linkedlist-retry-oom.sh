#!/bin/bash
# Fail replacement-node allocation for a borrowed transactional BE lease after
# cooperative FORCE_TERM. The shared A1/Bempty oracle and final distinction
# between retry discard and shutdown discard prove one source disposition.
export LOCAL_QUEUE_BORROWED_FORCE_TERM=1
export LOCAL_QUEUE_S6_RETRY_OOM=1
. ${srcdir:=.}/local-queue-be-force-term-actions.sh
