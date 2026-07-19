#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Shared DA save-on-shutdown/restart driver. The wrapper selects the memory
# queue and disk engine. Single-record, deliberately slowed dequeue leaves a
# durable spill at immediate shutdown; restart must recover every sequence.
# Duplicates are accepted because an interrupted in-flight batch is replayable.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2000
DA_ENGINE=${DA_ENGINE:?DA_ENGINE must be auto or disk}
DA_QUEUE_TYPE=${DA_QUEUE_TYPE:?DA_QUEUE_TYPE must be LinkedList or FixedArray}
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
idle_config=
if [ "$DA_ENGINE" = auto ]; then
	idle_config='queue.diskQueueIdleTimeout="-1"'
fi

generate_conf
# shellcheck disable=SC2090 # RainerScript quotes are intentionally retained.
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="'"$DA_QUEUE_TYPE"'" queue.filename="mainq"
	queue.size="6000" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.dequeueSlowdown="10000"
	queue.timeoutShutdown="1000"
	queue.saveOnShutdown="on"
	queue.diskQueueType="'"$DA_ENGINE"'" '"$idle_config"')
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'

startup
injectmsg
shutdown_immediate
wait_shutdown
if [ "$DA_ENGINE" = auto ]; then
	[ -s "$SPOOL_DIR/mainq.segq/state" ] ||
		error_exit 1 "segmented DA save-on-shutdown did not persist state"
	content_check "RSYSLOG-DA-ENGINE-V1 segmentedDisk" "$SPOOL_DIR/mainq.da-engine"
else
	check_mainq_spool
fi
startup
# Main-queue emptiness may be observed while the DA child is still draining.
# Sequence completion is therefore the phase-two oracle and shutdown barrier.
wait_seq_check 0 $((NUMMESSAGES - 1)) -d
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
exit_test
