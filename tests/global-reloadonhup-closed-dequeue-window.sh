#!/bin/bash
# A worker blocked outside its dequeue window must acknowledge a reload
# batch barrier promptly. imdiag startup readiness precedes message injection;
# the exact rate-limiter debug marker then proves the main-queue worker reached
# its closed window before HUP. Debug output is required here because the
# blocked main queue cannot emit a diagnostic through its normal omfile path.
# The next window starts current local hour + 2 (modulo 24), at least about an
# hour away. Harness timeouts are deadlock guards, not scheduling oracles.
# Activated generation 2 proves the barrier completed; no message delivery is
# claimed. Immediate shutdown and wait_shutdown must prove proper termination
# without waiting for the intentionally undrained queue to become empty.
. ${srcdir:=.}/diag.sh init
export RSYSLOG_DEBUG="debug nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.window-debug.log"

window_hour=$(date +%H)
window_begin=$(((10#$window_hour + 2) % 24))
window_end=$(((window_begin + 1) % 24))
generate_conf
add_conf '
global(processInternalMessages="off" config.reloadOnHUP="on" compactJsonString="off")
main_queue(queue.type="LinkedList" queue.workerThreads="1"
    queue.dequeueTimeBegin="'$window_begin'" queue.dequeueTimeEnd="'$window_end'")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg 0 1
wait_content 'outside dequeue time window, delaying' "$RSYSLOG_DEBUGLOG"

sed 's/compactJsonString="off"/compactJsonString="on"/' "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"

# Preserve the proper-termination oracle even when activation assertions fail.
shutdown_immediate
wait_shutdown
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ]]; then
    echo "FAIL: reload did not quiesce the closed-window worker: $reload_status"
    error_exit 1
fi
exit_test
