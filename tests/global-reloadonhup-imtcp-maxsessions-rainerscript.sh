#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify allocation-free imtcp maxSessions resizing under the reload fence.
# A held session fills the initial one-slot table and rsyslog's own rejection
# diagnostic proves the cap before HUP. Module- and input-level growth then
# admit one additional persistent session each without disconnecting older
# peers. Shrinking with an occupied truncated slot fails during the fenced
# runtime check; after that peer closes, the same candidate activates while
# preserving the two remaining sessions. The close notification is the
# deterministic oracle that the third slot is free before retry; a subsequent
# third-peer rejection proves the smaller table is active rather than a no-op
# commit. A growth across the next implicit-backlog bucket remains rejected.
# Values one through three share one bucket. The explicit-backlog wrapper
# additionally grows across that bucket and rejects a changed socket backlog.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf 'global(config.reloadOnHUP="on")'
add_conf 'module(load="../plugins/imtcp/.libs/imtcp" maxSessions="1")'
add_conf 'input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="sessions" notifyOnConnectionClose="on" ruleset="main")'
add_conf 'ruleset(name="main") {'
add_conf '  action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'")'
add_conf '}'
if [[ -n "${RSYSLOG_RELOAD_EXPLICIT_BACKLOG:-}" ]]; then
    sed -i 's/ruleset="main")/socketBacklog="10" ruleset="main")/' "${TESTCONF_NM}.conf"
fi
# Count exact lifecycle diagnostics: unrelated internal messages cannot satisfy
# the close/admission oracles while an event loop is still processing a peer.
close_notice_count() {
    awk '/closed by remote peer/ { ++count } END { print count + 0 }' "${RSYSLOG_DYNNAME}.started"
}
admission_reject_count() {
    awk '/too many tcp sessions - dropping incoming request/ { ++count } END { print count + 0 }' "${RSYSLOG_DYNNAME}.started"
}
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"

# The second handshake may complete from the kernel backlog, but SessAccept
# must reject it because the sole userspace slot is occupied.
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: rejected-before-grow\n' >&8 || error_exit 1
wait_content 'too many tcp sessions - dropping incoming request' "${RSYSLOG_DYNNAME}.started"
exec 8>&-

cp "$CONF_FILE" "$CONF_FILE.startup"
sed 's/maxSessions="1"/maxSessions="2"/' "$CONF_FILE.startup" >"$CONF_FILE.module-grow"
cp "$CONF_FILE.module-grow" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: module maxSessions growth did not activate: $reload_status"
	error_exit 1
fi
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: module-grow-old\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: module-grow-new\n' >&8 || error_exit 1
wait_content 'module-grow-old' "$RSYSLOG_OUT_LOG"
wait_content 'module-grow-new' "$RSYSLOG_OUT_LOG"

sed 's/ruleset="main")/maxSessions="3" ruleset="main")/' "$CONF_FILE.module-grow" >"$CONF_FILE.input-grow"
cp "$CONF_FILE.input-grow" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: input maxSessions growth did not activate: $reload_status"
	error_exit 1
fi
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: input-grow-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: input-grow-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: input-grow-third\n' >&7 || error_exit 1
wait_content 'input-grow-first' "$RSYSLOG_OUT_LOG"
wait_content 'input-grow-second' "$RSYSLOG_OUT_LOG"
wait_content 'input-grow-third' "$RSYSLOG_OUT_LOG"

sed 's/maxSessions="3"/maxSessions="2"/' "$CONF_FILE.input-grow" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=3"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
    echo "FAIL: occupied-tail maxSessions shrink was not rejected at the fence: $reload_status"
    error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: shrink-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: shrink-kept-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: shrink-kept-third\n' >&7 || error_exit 1
wait_content 'shrink-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'shrink-kept-second' "$RSYSLOG_OUT_LOG"
wait_content 'shrink-kept-third' "$RSYSLOG_OUT_LOG"

close_line_count=$(close_notice_count "${RSYSLOG_DYNNAME}.started")
exec 7>&-
wait_file_lines --count-function close_notice_count "${RSYSLOG_DYNNAME}.started" $((close_line_count + 1))

# The same two-slot candidate now fits every active session and must commit.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
    echo "FAIL: eligible maxSessions shrink did not activate: $reload_status"
    error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: shrink-active-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: shrink-active-second\n' >&8 || error_exit 1
wait_content 'shrink-active-first' "$RSYSLOG_OUT_LOG"
wait_content 'shrink-active-second' "$RSYSLOG_OUT_LOG"

# The original one-slot cap produced exactly one rejection. A second rejection
# after the shrink proves the committed runtime table now has only two slots.
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: rejected-after-shrink\n' >&7 || error_exit 1
wait_file_lines --count-function admission_reject_count "${RSYSLOG_DYNNAME}.started" 2
content_count_check 'too many tcp sessions - dropping incoming request' 2 "${RSYSLOG_DYNNAME}.started"
exec 7>&-

sed 's/maxSessions="3"/maxSessions="10"/' "$CONF_FILE.input-grow" >"$CONF_FILE"
final_generation=4
if [[ -n "${RSYSLOG_RELOAD_EXPLICIT_BACKLOG:-}" ]]; then
    # An explicit unchanged backlog permits growth across an implicit bucket.
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"result=activated active_generation=5"* ||
          "$reload_status" != *"source_capability=live_swap"* ]]; then
        echo "FAIL: explicit-backlog growth did not activate: $reload_status"
        error_exit 1
    fi
    exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
    printf '<167>Mar 10 01:00:00 host app: explicit-backlog-new-slot\n' >&7 || error_exit 1
    wait_content 'explicit-backlog-new-slot' "$RSYSLOG_OUT_LOG"
    exec 7>&-
    final_generation=5
    # Changing the actual listen backlog is still a restart boundary.
    sed -i 's/socketBacklog="10"/socketBacklog="11"/' "$CONF_FILE"
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=$final_generation"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: effective backlog change was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: backlog-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: backlog-kept-second\n' >&8 || error_exit 1
wait_content 'backlog-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'backlog-kept-second' "$RSYSLOG_OUT_LOG"

# Listener pointer-table capacity is deliberately outside this delivery.
sed 's/ruleset="main")/maxListeners="30" ruleset="main")/' "$CONF_FILE.input-grow" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=$final_generation"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
    echo "FAIL: maxListeners change escaped the restart boundary: $reload_status"
    error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: listener-capacity-kept\n' >&9 || error_exit 1
wait_content 'listener-capacity-kept' "$RSYSLOG_OUT_LOG"

exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
