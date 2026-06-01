#!/bin/bash
# Test for issue #2646: disk-assisted queue must drain after restart with no new traffic.
#
# Three root causes fixed:
#  1. pqDA inherited iDeqSlowdown from parent, throttling recovery.
#  2. pqDA inherited a small iDeqBatchSize (as low as 8), requiring thousands
#     of lock cycles per second for large queues.
#  3. batchProcessed() did not re-advise pqDA workers; with no concurrent
#     enqueue traffic the workers could retire between batches and never restart.
#
# Scenario:
#  Phase 1: fill the disk portion of a DA queue with a slow action, then
#           shut down immediately so messages stay on disk.
#  Phase 2: restart with a fast action and no new traffic; verify the disk
#           queue drains completely.
#
# added 2026-06-01 by contributor
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

SPOOL_DIR="${RSYSLOG_DYNNAME}.spool"
export NUMMESSAGES=3000

# -- Phase 1 config: slow action forces messages to spill onto disk ----------
write_phase1_conf() {
    generate_conf
    add_conf '
module(load="../plugins/imdiag/.libs/imdiag")
module(load="../plugins/omtesting/.libs/omtesting")

global(workDirectory="'"$SPOOL_DIR"'")

main_queue(
    queue.type="linkedList"
    queue.filename="mainq"
    queue.size="500"
    queue.highWatermark="400"
    queue.lowWatermark="100"
    queue.maxDiskSpace="50m"
    queue.saveOnShutdown="on"
    queue.timeoutShutdown="1"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

# Slow action: each batch sleeps 200 ms, causing messages to overflow to disk.
action(type="omtesting" name="slow-sink" template="outfmt"
       delay.begin="0" delay.step="200")
'
}

# -- Phase 2 config: fast action, no new traffic -----------------------------
write_phase2_conf() {
    generate_conf
    add_conf '
module(load="../plugins/imdiag/.libs/imdiag")

global(workDirectory="'"$SPOOL_DIR"'")

main_queue(
    queue.type="linkedList"
    queue.filename="mainq"
    queue.size="500"
    queue.highWatermark="400"
    queue.lowWatermark="100"
    queue.maxDiskSpace="50m"
    queue.saveOnShutdown="on"
)

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'
}

# ============================================================
# Phase 1: inject messages, overflow disk queue, stop.
# ============================================================
mkdir -p "$SPOOL_DIR"
write_phase1_conf
startup
injectmsg 0 $NUMMESSAGES
# Stop immediately; do not wait for drain so disk queue keeps the backlog.
shutdown_immediate
wait_shutdown

# Sanity: confirm queue files exist.
if ! ls "$SPOOL_DIR"/mainq.0000* >/dev/null 2>&1; then
    echo "FAIL: no queue files found after phase 1 shutdown"
    error_exit 1
fi
echo "Phase 1 done: messages persisted to $SPOOL_DIR"

# ============================================================
# Phase 2: restart with no new traffic; disk queue must drain.
# ============================================================
echo "Phase 2: restarting without new traffic — disk queue must self-drain"
write_phase2_conf
startup
# Do NOT inject any new messages.  shutdown_when_empty waits until the
# queue (including the DA disk portion) reports empty.
shutdown_when_empty
wait_shutdown

# Verify every message was delivered exactly once.
seq_check 0 $((NUMMESSAGES - 1))

echo "SUCCESS: DA queue drained without incoming traffic"
exit_test
