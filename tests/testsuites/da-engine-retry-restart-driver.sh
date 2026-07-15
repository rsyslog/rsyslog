#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Shared DA retry/restart driver. The wrapper selects the memory queue and disk
# engine. A deliberately suspended omfile action must spill, survive an
# immediate restart, and preserve retry-before-commit semantics. Exact sequence
# coverage after the destination becomes writable proves no loss; duplicates
# are accepted because a shutdown may replay an in-flight action batch.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
DA_ENGINE=${DA_ENGINE:?DA_ENGINE must be auto or disk}
DA_QUEUE_TYPE=${DA_QUEUE_TYPE:?DA_QUEUE_TYPE must be LinkedList or FixedArray}
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
OUTPUT_DIR="$PWD/${RSYSLOG_DYNNAME}.missing"
OUTPUT_FILE="$OUTPUT_DIR/output.log"

wait_for_store() {
	local deadline=$((SECONDS + 30))
	while true; do
		if [ "$DA_ENGINE" = auto ]; then
			[ -s "$SPOOL_DIR/retryq.segq/state" ] && return
		else
			[ -s "$SPOOL_DIR/retryq.00000001" ] && return
		fi
		[ "$SECONDS" -lt "$deadline" ] ||
			error_exit 1 "$DA_ENGINE DA store was not materialized"
		./msleep 25
	done
}

idle_config=
if [ "$DA_ENGINE" = auto ]; then
	idle_config='queue.diskQueueIdleTimeout="-1"'
fi

generate_conf
# shellcheck disable=SC2090 # RainerScript quotes are intentionally retained.
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="Direct")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$OUTPUT_FILE"'" template="outfmt"
	createDirs="off" action.resumeRetryCount="-1" action.resumeInterval="1"
	queue.type="'"$DA_QUEUE_TYPE"'" queue.filename="retryq"
	queue.size="2000" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.timeoutShutdown="10000" queue.saveOnShutdown="on"
	queue.diskQueueType="'"$DA_ENGINE"'" '"$idle_config"')
'

startup
injectmsg
# The direct main queue makes completion of injectmsg the barrier proving that
# every event reached the suspended action queue before immediate shutdown.
wait_for_store
shutdown_immediate
wait_shutdown

mkdir "$OUTPUT_DIR"
startup
wait_file_lines "$OUTPUT_FILE" "$NUMMESSAGES" 300
shutdown_when_empty
wait_shutdown
RSYSLOG_OUT_LOG="$OUTPUT_FILE" seq_check 0 $((NUMMESSAGES - 1)) -d
exit_test
