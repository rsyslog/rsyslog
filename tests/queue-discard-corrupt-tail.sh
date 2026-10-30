#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A classic Disk queue restores two records plus one stale .qi tail entry.
# Dequeue discards both real records before reaching the write pointer. The
# unread tail and the discarded batch must each leave the imdiag count once.
# The disposal handshake preserves the two records before restart; a bounded
# queue-empty acknowledgement and clean shutdown are the completion oracles.
# Exact output proves the recovered records were discarded. Timeouts diagnose
# missing handshakes or a stuck counter.
. ${srcdir:=.}/diag.sh init
skip_platform "AIX" "test requires shared-library interposition"
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
check_command_available timeout
export RSYSLOG_QUEUE_DISPOSAL_READY="$PWD/$RSYSLOG_DYNNAME.ready"
export RSYSLOG_QUEUE_DISPOSAL_RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
export RSYSLOG_PRELOAD=.libs/liboverride_queue_disposal.so
mkfifo "$RSYSLOG_QUEUE_DISPOSAL_RELEASE"
SPOOL_DIR="$RSYSLOG_DYNNAME.spool"
mkdir -p "$SPOOL_DIR"
generate_conf
add_conf '
global(processInternalMessages="off" workDirectory="'"$SPOOL_DIR"'")
main_queue(queue.type="Disk" queue.filename="dq" queue.size="100"
    queue.workerThreads="1" queue.dequeueBatchSize="32"
    queue.checkpointInterval="1" queue.saveOnShutdown="on"
    queue.onCorruption="ignore" queue.discardMark="3" queue.discardSeverity="0")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    if ($msg contains "msgnum:00000000:") then
        set $!queue_disposal_gate = "held";
    action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
}
'
startup
injectmsg 0 1
wait_file_lines "$RSYSLOG_QUEUE_DISPOSAL_READY" 1 10
printf 'injectmsg 1 2\n' | timeout 10 "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT" || error_exit 1
RSTB_EXPECT_NO_PROPER_TERMINATION=YES
wait_rsyslog_instance_pid
cp "$RSYSLOG_PIDBASE.pid" "$RSYSLOG_PIDBASE.pid.save"
kill -9 "$(cat "$RSYSLOG_PIDBASE.pid")"
wait_shutdown "" 10
rm -f "$RSYSLOG_PIDBASE.pid"
unset RSTB_EXPECT_NO_PROPER_TERMINATION
# The checkpoint must contain the two unread records; claim one more record
# than the persisted write pointer permits to exercise invalid-tail recovery.
content_check '+iQueueSize:2:1:2:' "$SPOOL_DIR/dq.qi"
sed 's/^+iQueueSize:2:1:2:$/+iQueueSize:2:1:3:/' "$SPOOL_DIR/dq.qi" > "$SPOOL_DIR/dq.qi.new"
mv "$SPOOL_DIR/dq.qi.new" "$SPOOL_DIR/dq.qi"
unset RSYSLOG_PRELOAD
startup
printf 'WaitMainQueueEmpty\n' | timeout 10 "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT" || error_exit 1
shutdown_when_empty
wait_shutdown "" 10
export EXPECTED='00000000'
cmp_exact
exit_test
