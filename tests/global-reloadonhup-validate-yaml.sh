#!/bin/bash
# Verify native YAML reloadOnHUP=validate parses a changed candidate without
# constructing its action, and leaves the active route unchanged. The same
# daemon and listener must continue writing through the old YAML ruleset.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "validate"' "${TESTCONF_NM}.yaml"
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
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
tcpflood -m1 -i0
wait_queueempty
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
# A normal global-object dispatch would create/truncate this file. The YAML
# candidate must be captured without executing that setter.
sed -i '/config.reloadOnHUP:/a\  debug.logFile: "'$RSYSLOG_DYNNAME'.reload-sentinel"' "$CONF_FILE"
cp "$CONF_FILE" "$CONF_FILE.valid"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=validated_syntax_only active_generation=1"* ]]; then
	echo "FAIL: unexpected reload status: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"
tcpflood -m1 -i1
wait_queueempty

# Exercise parser recovery in the same daemon. A malformed native YAML
# candidate must be rejected, and a later record on the active listener must
# still use the old action. HUP generation and getreloadstatus avoid sleeps.
printf 'version: 2\nglobal: [\n' >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_syntax_invalid active_generation=1"* ]]; then
	echo "FAIL: unexpected invalid-candidate status: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i2
wait_queueempty

# Restore the valid YAML candidate to prove parser recovery after the rejected
# document. The active output remains unchanged because validate never commits.
cp "$CONF_FILE.valid" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=validated_syntax_only active_generation=1"* ]]; then
	echo "FAIL: YAML parser did not recover after invalid candidate: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"
tcpflood -m1 -i3
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
