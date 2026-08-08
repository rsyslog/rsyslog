#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Exercise rsyslog-segqueue against a store written by the real queue engine.
# A deliberately slow main queue is stopped with saveOnShutdown enabled, which
# must leave a non-empty segmented queue.  The tool must validate that store and
# export structurally valid JSONL while rsyslog is offline.  Restart and the
# normal sequence oracle then prove that inspection did not alter replay data.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=2000
SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
QUEUE_DIR="$SPOOL_DIR/mainq.segq"
SEGQUEUE_TOOL="$srcdir/../tools/rsyslog-segqueue"
EXPORT_FILE="${RSYSLOG_DYNNAME}.jsonl"

command -v python3 >/dev/null 2>&1 || skip_platform "python3 not available"

generate_conf
add_conf '
global(workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="LinkedList" queue.filename="mainq"
	queue.size="6000" queue.highWatermark="10" queue.lowWatermark="5"
	queue.dequeueBatchSize="1" queue.dequeueSlowdown="10000"
	queue.timeoutShutdown="1000" queue.saveOnShutdown="on"
	queue.diskQueueType="auto" queue.diskQueueIdleTimeout="-1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'

startup
injectmsg
shutdown_immediate
wait_shutdown

[ -s "$QUEUE_DIR/state" ] || error_exit 1 "segmented queue state was not persisted"
python3 "$SEGQUEUE_TOOL" check "$QUEUE_DIR" || error_exit 1 "tool rejected a runtime-written queue"
python3 "$SEGQUEUE_TOOL" status --json "$QUEUE_DIR" >"${RSYSLOG_DYNNAME}.status.json" ||
	error_exit 1 "tool status failed for a runtime-written queue"
python3 "$SEGQUEUE_TOOL" export --scope live --output "$EXPORT_FILE" "$QUEUE_DIR" ||
	error_exit 1 "tool export failed for a runtime-written queue"
python3 - "$EXPORT_FILE" <<'PY' || error_exit 1 "tool export was not valid JSONL"
import json
import sys

count = 0
with open(sys.argv[1], "r", encoding="utf-8") as stream:
    for line in stream:
        record = json.loads(line)
        assert isinstance(record["queue"]["record_sequence"], int)
        assert isinstance(record["message"], dict)
        count += 1
assert count > 0
PY

startup
# Main-queue emptiness may be observed while the DA child is still draining.
# Complete sequence recovery is the non-mutation oracle for the tool run.
wait_seq_check 0 $((NUMMESSAGES - 1)) -d
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -d
exit_test
