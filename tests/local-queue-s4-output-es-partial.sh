#!/bin/bash
# Force actual omelasticsearch maxbytes flushes within a four-element helper
# transaction. PREVIOUS_COMMITTED prefixes must not be replayed when a later
# item is rejected. The shared fixture checks exact HTTP and retry inventories.
export LOCAL_QUEUE_ES_MAXBYTES=1
. ${srcdir:=.}/local-queue-s4-output-es.sh
