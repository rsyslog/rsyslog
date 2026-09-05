#!/bin/bash
# Hold the final JSON destructor while the empty-queue worker is unlocked and
# not registered as a waiter, then request shutdown. The debug marker proves
# shutdown has set the pool state and sent its wakeup before disposal resumes.
# Bounded proper termination proves the worker rechecks stop state after this
# unlock instead of losing shutdown's wakeup. Limits diagnose a deadlock;
# ordering comes from the FIFO and the post-signal shutdown marker.
. ${srcdir:=.}/diag.sh init
skip_platform "AIX" "test requires shared-library interposition"
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
check_command_available timeout
export RSYSLOG_QUEUE_DISPOSAL_READY="$PWD/$RSYSLOG_DYNNAME.ready"
export RSYSLOG_QUEUE_DISPOSAL_RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
export RSYSLOG_PRELOAD=.libs/liboverride_queue_disposal.so
export RSYSLOG_DEBUG="Debug"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
mkfifo "$RSYSLOG_QUEUE_DISPOSAL_RELEASE"
generate_conf
add_conf '
global(processInternalMessages="off")
main_queue(queue.type="FixedArray" queue.size="128"
    queue.workerThreads="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="20000")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    set $!queue_disposal_gate = "held";
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg 0 1
wait_file_lines "$RSYSLOG_QUEUE_DISPOSAL_READY" 1 10
shutdown_immediate
TB_TEST_TIMEOUT=10 wait_content 'main Q:Reg: waiting .* on worker thread termination' "$RSYSLOG_DEBUGLOG"
timeout 10 bash -c 'printf "release\n" > "$RSYSLOG_QUEUE_DISPOSAL_RELEASE"' || error_exit 1
wait_shutdown "" 10
seq_check 0 0
exit_test
