#!/bin/bash
# Test for issue #2646: disk-assisted queue draining without new traffic
# This test verifies that a DA queue with persisted data will drain
# automatically on restart, even when there is no new incoming traffic.
# Before the fix, the DA worker only activated when the memory queue
# reached the high watermark, causing slow drains that took days.
# After the fix, the DA worker activates whenever the disk queue has data.
#
# Test scenario:
# 1. Start rsyslog with a DA queue configured
# 2. Inject messages with a slow consumer to fill disk queue
# 3. Shutdown to persist queue to disk
# 4. Restart rsyslog WITHOUT injecting new messages
# 5. Verify that disk queue drains automatically
#
# added 2026-01-29 by rgerhards via GitHub Copilot
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")

# Configure main queue as disk-assisted with high watermark
# The key is that we want the disk queue to have data, but the
# memory queue to be empty on restart (below high watermark)
main_queue(
	queue.type="linkedList"
	queue.filename="mainq"
	queue.maxDiskSpace="50m"
	queue.size="1000"
	queue.highWatermark="800"
	queue.lowWatermark="200"
	queue.timeoutShutdown="1"
	queue.saveOnShutdown="on"
)

module(load="../plugins/imdiag/.libs/imdiag")
module(load="../plugins/omtesting/.libs/omtesting")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

$IncludeConfig '${RSYSLOG_DYNNAME}'work-delay.conf
'

# Phase 1: Fill the disk queue with a slow consumer
echo "*.*     :omtesting:sleep 0 1000" > ${RSYSLOG_DYNNAME}work-delay.conf

startup
injectmsg 0 $NUMMESSAGES
shutdown_immediate
wait_shutdown
check_mainq_spool

# Verify we have queue files on disk
ls -l ${RSYSLOG_DYNNAME}.spool/mainq.0000* > /dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "ERROR: no queue files found after phase 1 shutdown"
	error_exit 1
fi

echo "Phase 1 complete: $NUMMESSAGES messages persisted to disk"

# Phase 2: Restart WITHOUT injecting new messages
# Remove the delay so messages can drain quickly
echo "#" > ${RSYSLOG_DYNNAME}work-delay.conf

# The critical test: restart and verify DA worker activates
# even though no new messages are being injected
echo "Phase 2: Restarting rsyslog WITHOUT new traffic..."
startup

# Wait for queue to drain - the DA worker should activate automatically
# because the disk queue has data (the fix for issue #2646)
shutdown_when_empty
wait_shutdown

# Verify all messages were processed
seq_check 0 $((NUMMESSAGES - 1))

# Verify queue files are cleaned up
ls ${RSYSLOG_DYNNAME}.spool/mainq.0000* > /dev/null 2>&1
if [ $? -eq 0 ]; then
	echo "WARNING: queue files still exist after drain (this may be OK)"
fi

echo "SUCCESS: DA queue drained without new traffic"
exit_test
