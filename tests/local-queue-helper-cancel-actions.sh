#!/bin/bash
# Cancel a helper inside the real transactional dispatch gate with populated
# parameters and a BE-source lease. No FIFO release means the return path is
# pthread cancellation, not cooperative FORCE_TERM; final conservation checks
# run after private state disposal and all workers have joined.
export LOCAL_QUEUE_BORROWED_FORCE_TERM=1
export LOCAL_QUEUE_BORROWED_CANCEL=1
. ${srcdir:=.}/local-queue-be-force-term-actions.sh
