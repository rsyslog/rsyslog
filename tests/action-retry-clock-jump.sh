#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression for issue #5245: a suspended omfwd action must retry after its
# configured interval even when the wall clock moves 30 days backward.
# The receiver owns its port before rsyslog starts, but listens only after
# suspension is observed in the action debug log. An action with no queued
# work does not wake itself, so a second message after 11 real seconds drives
# the retry. Delivery within 10 more seconds proves recovery. The 10-second
# interval leaves time to make the jump before the old deadline expires.
# RSYSLOG_ACTION_RETRY_CLOCK_SHIFT=none is a local control of the same path.
. ${srcdir:=.}/diag.sh init
. $srcdir/faketime_common.sh

export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"
export FAKETIME_TIMESTAMP_FILE="$PWD/$RSYSLOG_DYNNAME.faketime"
export FAKETIME_NO_CACHE=1
export FAKETIME_DONT_FAKE_MONOTONIC=1
printf '+0\n' > "$FAKETIME_TIMESTAMP_FILE"

LISTEN_RELEASE="$RSYSLOG_DYNNAME.minitcpsrvr.listen"
LISTEN_READY="$RSYSLOG_DYNNAME.minitcpsrvr.ready"
rm -f "$LISTEN_RELEASE" "$LISTEN_READY"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1 "$LISTEN_RELEASE" "$LISTEN_READY"

generate_conf
add_conf '
global(debug.whitelist="on"
       debug.files=["action.c", "../action.c", "../../runtime/action.c", "../../../runtime/action.c"])
template(name="outfmt" type="string" string="%msg%\n")
if $msg contains "msgnum:" then {
    action(name="forwarder" type="omfwd" target="127.0.0.1"
           port="'$MINITCPSRVR_PORT1'" protocol="tcp" template="outfmt"
           queue.type="LinkedList" queue.dequeueBatchSize="1"
           action.resumeRetryCount="0" action.resumeInterval="10")
}
'
startup
injectmsg 0 1
wait_content "action 'forwarder' suspended" "$RSYSLOG_DEBUGLOG"

if [ "${RSYSLOG_ACTION_RETRY_CLOCK_SHIFT:-backward}" = backward ]; then
    printf '%s\n' '-30d' > "$FAKETIME_TIMESTAMP_FILE.new"
    mv "$FAKETIME_TIMESTAMP_FILE.new" "$FAKETIME_TIMESTAMP_FILE"
fi
touch "$LISTEN_RELEASE"
wait_file_exists "$LISTEN_READY"
$TESTTOOL_DIR/msleep 11000
injectmsg 1 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1 10

shutdown_when_empty
wait_shutdown
content_check "msgnum:00000001" "$RSYSLOG_OUT_LOG"
exit_test
