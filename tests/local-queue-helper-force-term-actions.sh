#!/bin/bash
# A helper owns the transactional BE lease while the sole dedicated consumer
# is blocked. Reuse the real SUSPENDED -> actionCommit FORCE_TERM/reset seam;
# joined helper cleanup must reinsert into BE without FE transfer accounting,
# then bounded family shutdown explicitly discards the remaining obligations.
export LOCAL_QUEUE_BORROWED_FORCE_TERM=1
. ${srcdir:=.}/local-queue-be-force-term-actions.sh
