#!/bin/bash
# Exercise a helper's cooperative interrupted BE transaction and successful
# BE replay, with no forced reader cancellation. After the real FORCE_TERM /
# currIParam-reset marker, release the dedicated BE reader. Require A=1,2/B=2,
# one borrowed BE return, zero FE transfers/discards, joined workers and full
# final conservation. This variant retains TSan source-switch coverage while
# separate normal/ASan fixtures cover its known read-cancellation limitation.
export LOCAL_QUEUE_BORROWED_FORCE_TERM=1
export LOCAL_QUEUE_BORROWED_REPLAY=1
. ${srcdir:=.}/local-queue-be-force-term-actions.sh
