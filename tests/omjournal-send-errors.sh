#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify negative journal API returns reach the action retry/fallback machinery
# for ordinary and JSON-template sends. Each queued action gets EAGAIN once;
# an exact second call with success proves retry rather than silent commit.
# Separate direct actions always receive EINVAL/EMSGSIZE; their fallback files
# prove failure propagation without waiting forever for a permanent error.
# Success controls follow retry messages through the same queued actions.
# Shutdown/queue drain synchronizes the oracle; there are no timing sleeps.
# Transport calls are interposed, so this test needs no journal service.
. ${srcdir:=.}/diag.sh init
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
require_plugin omjournal
export RSYSLOG_JOURNAL_SEND_EVENTS="$PWD/$RSYSLOG_DYNNAME.events"
export RSYSLOG_PRELOAD="./.libs/libomjournal_send_errors_preload.so"
generate_conf
add_conf '
module(load="../plugins/omjournal/.libs/omjournal")
template(name="journalfields" type="list") {
    property(name="msg" outname="MESSAGE")
}
template(name="fallback" type="string" string="%msg%\n")
'
for mode in plain template; do
    template_arg=''
    [ "$mode" = template ] && template_arg='template="journalfields"'
    add_conf '
if ($msg contains "journal-test-'"$mode"'-retry" or
    $msg contains "journal-test-'"$mode"'-success") then {
    action(type="omjournal" '"$template_arg"'
        action.resumeRetryCount="1" action.resumeInterval="1"
        queue.type="LinkedList" queue.dequeueBatchSize="1")
    stop
}'
    for outcome in invalid oversize; do
        add_conf '
if ($msg contains "journal-test-'"$mode-$outcome"'") then {
    action(type="omjournal" '"$template_arg"' action.resumeRetryCount="0")
    action(type="omfile" file="'"$RSYSLOG_DYNNAME.fallback"'" template="fallback"
        action.execOnlyWhenPreviousIsSuspended="on")
    stop
}'
    done
done
add_conf 'action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'")'
startup
for mode in plain template; do
    for outcome in retry invalid oversize success; do
        injectmsg_literal "<167>Mar  1 01:00:00 host tag: journal-test-$mode-$outcome"
    done
done
shutdown_when_empty
wait_shutdown
for api in send sendv; do
    mode=plain
    [ "$api" = sendv ] && mode=template
    cat >> "$RSYSLOG_DYNNAME.expected" <<EOF
$api journal-test-$mode-retry -11
$api journal-test-$mode-retry 0
$api journal-test-$mode-invalid -22
$api journal-test-$mode-oversize -90
$api journal-test-$mode-success 0
EOF
    for outcome in invalid oversize; do
        echo " journal-test-$mode-$outcome" >> "$RSYSLOG_DYNNAME.fallback.expected"
    done
done
sort "$RSYSLOG_DYNNAME.expected" > "$RSYSLOG_DYNNAME.expected.sorted"
sort "$RSYSLOG_JOURNAL_SEND_EVENTS" > "$RSYSLOG_DYNNAME.events.sorted"
cmp "$RSYSLOG_DYNNAME.expected.sorted" "$RSYSLOG_DYNNAME.events.sorted" || error_exit 1
cmp "$RSYSLOG_DYNNAME.fallback.expected" "$RSYSLOG_DYNNAME.fallback" || error_exit 1
content_check 'omjournal: journal submission failed: Invalid argument' "$RSYSLOG_OUT_LOG"
content_check 'omjournal: journal submission failed: Message too long' "$RSYSLOG_OUT_LOG"
exit_test
