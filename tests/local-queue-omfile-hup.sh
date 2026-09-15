#!/bin/bash
# A qualified file is reopened only by the main-thread HUP owner. Rotate the
# witness output too: its newly created file acknowledges HUP progressed past
# the primary output, avoiding any assumption about signal-delivery latency.
# After a failed HUP, restore a creatable pathname and process a witnessed probe;
# the primary file must remain absent (no worker lazy open). A later successful
# HUP and final delivery prove the same action can recover explicitly.
# Disable BE helping so the final BE probe uses a dedicated worker whose action
# was not suspended by the failed-open FE probe. This isolates HUP ownership and
# worker lazy-open behavior from helper scheduling and per-worker retry timers.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin imdiag
PRIMARY="$PWD/$RSYSLOG_DYNNAME.primary"
WITNESS="$PWD/$RSYSLOG_DYNNAME.witness"

wait_preopened_file() {
    local deadline=$(( $(date +%s) + 15 ))
    while [ ! -f "$1" ]; do
        [ "$(date +%s)" -lt "$deadline" ] || error_exit 1 "HUP did not prepare $1"
        "$TESTTOOL_DIR/msleep" 100
    done
}

generate_conf
# The local callback contract also applies to the harness diagnostic output.
sed -i '1i template(name="localdiag" type="string" string="%msg%\\n")' "${TESTCONF_NM}.conf"
sed -i "s|file=\"./$RSYSLOG_DYNNAME.started\"|file=\"$PWD/$RSYSLOG_DYNNAME.started\" template=\"localdiag\"|" "${TESTCONF_NM}.conf"
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="32"
    queue.local.frontendSize="4" queue.local.maxFrontends="1"
    queue.local.helperBatchSize="0")
template(name="hupmsg" type="string" string="%msg%\n")
if ($msg contains "sink-") then {
    action(type="omfile" file="'$PRIMARY'" template="hupmsg" queue.type="Direct")
    action(type="omfile" file="'$WITNESS'" template="hupmsg" queue.type="Direct")
}
'
startup
tcpflood -m1 -M'<167>Mar  1 01:00:00 host tag: sink-before-hup'
wait_file_lines "$PRIMARY" 1
wait_file_lines "$WITNESS" 1
mv "$PRIMARY" "$PRIMARY.before"
mv "$WITNESS" "$WITNESS.before"
mkdir "$PRIMARY"
issue_HUP
# Prepare creates an empty witness stream even with no new input. Observing it
# means HUP has processed the preceding primary action's failed open.
wait_preopened_file "$WITNESS"
rmdir "$PRIMARY"
tcpflood -m1 -M'<167>Mar  1 01:00:00 host tag: sink-after-failed-hup'
wait_file_lines "$WITNESS" 1
if [ -e "$PRIMARY" ]; then
    error_exit 1 'worker reopened primary file after failed HUP'
fi
mv "$WITNESS" "$WITNESS.failed"
issue_HUP
wait_preopened_file "$WITNESS"
wait_preopened_file "$PRIMARY"
injectmsg_literal '<167>Mar  1 01:00:00 host tag: sink-recovered'
wait_file_lines "$PRIMARY" 1
shutdown_when_empty
wait_shutdown
content_check 'sink-before-hup' "$PRIMARY.before"
content_check 'sink-after-failed-hup' "$WITNESS.failed"
content_check 'sink-recovered' "$PRIMARY"
exit_test
