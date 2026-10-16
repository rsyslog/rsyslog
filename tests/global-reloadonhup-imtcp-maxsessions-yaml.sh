#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native-YAML parity for fenced imtcp maxSessions resizing. A full initial table
# is proved by the server's own drop diagnostic. Module and input growth each
# add one usable slot while older sessions stay connected. An occupied-tail
# shrink fails at the fenced runtime check; after the third peer's close
# notification proves that slot is free, the same candidate activates and the
# remaining sessions survive. A subsequent third-peer rejection proves the
# smaller table is active rather than a no-op commit. A growth across the next
# implicit-backlog bucket remains restart-required. Values one through three
# share one bucket. The explicit-backlog wrapper also grows across that bucket
# and verifies that changing the actual socket backlog is rejected.
# A constant RainerScript include captures only internal diagnostics; every
# imtcp setting and reload candidate remains native YAML.
# The final maxListeners update is independent of the retained session table.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_DYNNAME.started" >"$RSYSLOG_DYNNAME.internal.conf"
add_yaml_conf 'include:'
add_yaml_conf '  - path: "'$RSYSLOG_DYNNAME'.internal.conf"'
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    maxSessions: 1'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    address: 127.0.0.1'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    name: sessions'
add_yaml_conf '    notifyOnConnectionClose: "on"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
if [[ -n "${RSYSLOG_RELOAD_EXPLICIT_BACKLOG:-}" ]]; then
    sed -i '/    ruleset: main/a\    socketBacklog: 10' "${TESTCONF_NM}.yaml"
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

exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: yaml-rejected-before-grow\n' >&8 || error_exit 1
wait_content 'too many tcp sessions - dropping incoming request' "${RSYSLOG_DYNNAME}.started"
exec 8>&-

cp "$CONF_FILE" "$CONF_FILE.startup"
sed 's/maxSessions: 1/maxSessions: 2/' "$CONF_FILE.startup" >"$CONF_FILE.module-grow"
cp "$CONF_FILE.module-grow" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML module maxSessions growth did not activate: $reload_status"
	error_exit 1
fi
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: yaml-module-grow-old\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-module-grow-new\n' >&8 || error_exit 1
wait_content 'yaml-module-grow-old' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-module-grow-new' "$RSYSLOG_OUT_LOG"

sed '/ruleset: main/a\    maxSessions: 3' "$CONF_FILE.module-grow" >"$CONF_FILE.input-grow"
cp "$CONF_FILE.input-grow" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: YAML input maxSessions growth did not activate: $reload_status"
	error_exit 1
fi
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: yaml-input-grow-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-input-grow-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-input-grow-third\n' >&7 || error_exit 1
wait_content 'yaml-input-grow-first' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-input-grow-second' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-input-grow-third' "$RSYSLOG_OUT_LOG"

sed 's/maxSessions: 3/maxSessions: 2/' "$CONF_FILE.input-grow" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=3"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
    echo "FAIL: YAML occupied-tail maxSessions shrink was not rejected at the fence: $reload_status"
    error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-kept-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-kept-third\n' >&7 || error_exit 1
wait_content 'yaml-shrink-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-shrink-kept-second' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-shrink-kept-third' "$RSYSLOG_OUT_LOG"

close_line_count=$(close_notice_count "${RSYSLOG_DYNNAME}.started")
exec 7>&-
wait_file_lines --count-function close_notice_count "${RSYSLOG_DYNNAME}.started" $((close_line_count + 1))

issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4"* ||
      "$reload_status" != *"modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
    echo "FAIL: YAML eligible maxSessions shrink did not activate: $reload_status"
    error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-active-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-active-second\n' >&8 || error_exit 1
wait_content 'yaml-shrink-active-first' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-shrink-active-second' "$RSYSLOG_OUT_LOG"

exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: yaml-rejected-after-shrink\n' >&7 || error_exit 1
wait_file_lines --count-function admission_reject_count "${RSYSLOG_DYNNAME}.started" 2
content_count_check 'too many tcp sessions - dropping incoming request' 2 "${RSYSLOG_DYNNAME}.started"
exec 7>&-

sed 's/maxSessions: 3/maxSessions: 10/' "$CONF_FILE.input-grow" >"$CONF_FILE"
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
    sed -i 's/socketBacklog: 10/socketBacklog: 11/' "$CONF_FILE"
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=$final_generation"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: YAML effective backlog change was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: yaml-backlog-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-backlog-kept-second\n' >&8 || error_exit 1
wait_content 'yaml-backlog-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-backlog-kept-second' "$RSYSLOG_OUT_LOG"

# Listener capacity can change independently without losing session traffic.
sed '/    ruleset: main/a\    maxListeners: 30' "$CONF_FILE.input-grow" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=$((final_generation + 1))"* ||
      "$reload_status" != *"source_capability=live_swap"* ]]; then
    echo "FAIL: compatible maxListeners update did not activate: $reload_status"
    error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: listener-capacity-kept\n' >&9 || error_exit 1
wait_content 'listener-capacity-kept' "$RSYSLOG_OUT_LOG"

exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
