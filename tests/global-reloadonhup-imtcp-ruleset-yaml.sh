#!/bin/bash
# Verify native YAML parity for moving one established imtcp session between
# existing rulesets. Separate action outputs make the session-pointer update
# observable without relying on queue timing or reconnect behavior.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: main_sink'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
add_yaml_conf '  - name: alternate'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        name: alternate_sink'
add_yaml_conf '        file: "'$RSYSLOG2_OUT_LOG'"'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: route-main-before\n' >&9 || error_exit 1
wait_content 'route-main-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"

sed '0,/ruleset: main/s//ruleset: alternate/' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML session ruleset update did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: route-alternate\n' >&9 || error_exit 1
wait_content 'route-alternate' "$RSYSLOG2_OUT_LOG"

cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML session ruleset restore did not activate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: route-main-after\n' >&9 || error_exit 1
wait_content 'route-main-after' "$RSYSLOG_OUT_LOG"
assert_content_missing 'route-alternate' "$RSYSLOG_OUT_LOG"
custom_assert_content_missing 'route-main-after' "$RSYSLOG2_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
