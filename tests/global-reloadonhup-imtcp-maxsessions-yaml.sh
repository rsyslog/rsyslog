#!/bin/bash
# Native-YAML parity for fenced imtcp maxSessions growth. A full initial table
# is proved by the server's own drop diagnostic. Module and input growth each
# add one usable slot while older sessions stay connected; shrinking remains
# restart-required and leaves all active sessions intact. The explicit socket
# backlog isolates the session-table change from listen(2) backlog semantics.
# A constant RainerScript include captures only internal diagnostics; every
# imtcp setting and reload candidate remains native YAML.
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
add_yaml_conf '    socketBacklog: 32'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
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

sed '/socketBacklog: 32/a\    maxSessions: 3' "$CONF_FILE.module-grow" >"$CONF_FILE.input-grow"
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
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3"* ||
      "$reload_status" != *"source_capability=restart_required"* ]]; then
	echo "FAIL: YAML maxSessions shrink was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-kept-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-kept-second\n' >&8 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: yaml-shrink-kept-third\n' >&7 || error_exit 1
wait_content 'yaml-shrink-kept-first' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-shrink-kept-second' "$RSYSLOG_OUT_LOG"
wait_content 'yaml-shrink-kept-third' "$RSYSLOG_OUT_LOG"

exec 7>&-
exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
