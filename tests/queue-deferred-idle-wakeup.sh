#!/bin/bash
# Pause final msg0 JSON destruction using an external-library interposer.
# While the worker is outside the queue mutex and not yet a queue waiter,
# enqueue msg1 and then release the destructor. Successful bounded admission
# proves destruction is unlocked; output of msg1 proves the idle path retests
# the queue after disposal instead of losing that enqueue's wakeup. FIFO
# readiness/release determines ordering; time limits only diagnose a deadlock.
# The test-only hook cannot coexist with ASan's runtime-first load order.
. ${srcdir:=.}/diag.sh init
skip_platform "AIX" "test requires shared-library interposition"
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
check_command_available timeout
export RSYSLOG_QUEUE_DISPOSAL_READY="$PWD/$RSYSLOG_DYNNAME.ready"
export RSYSLOG_QUEUE_DISPOSAL_RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
export RSYSLOG_PRELOAD=.libs/liboverride_queue_disposal.so
mkfifo "$RSYSLOG_QUEUE_DISPOSAL_RELEASE"
generate_conf
add_conf '
global(processInternalMessages="off")
main_queue(queue.type="FixedArray" queue.size="128"
    queue.workerThreads="1" queue.dequeueBatchSize="1")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    if ($msg contains "msgnum:00000000:") then
        set $!queue_disposal_gate = "held";
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg 0 1
wait_file_lines "$RSYSLOG_QUEUE_DISPOSAL_READY" 1 10
# Direct diagtalker permits a strict bound on admission while the destructor
# remains paused. A queued TCP send alone would not prove admission completed.
printf 'injectmsg 1 1\n' | timeout 10 "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT" || error_exit 1
timeout 10 bash -c 'printf "release\n" > "$RSYSLOG_QUEUE_DISPOSAL_RELEASE"' || error_exit 1
wait_file_lines "$RSYSLOG_OUT_LOG" 2 10
shutdown_when_empty
wait_shutdown
seq_check 0 1
exit_test
