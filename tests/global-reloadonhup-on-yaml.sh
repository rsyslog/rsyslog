#!/bin/bash
# Verify native YAML reloadOnHUP=on safely parses candidates and fails closed
# at both current boundaries: an eligible no-op reaches the unavailable
# activation phase, while an action change stops at the ruleset-only scope
# gate. Both numbered records must use the old active ruleset after HUP.
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
add_yaml_conf '    statements:'
add_yaml_conf '      - if: '\''$msg contains "msgnum"'\'''
add_yaml_conf '        action:'
add_yaml_conf '          type: omfile'
add_yaml_conf '          name: sink'
add_yaml_conf '          file: "'$RSYSLOG_OUT_LOG'"'
startup
tcpflood -m1 -i0
wait_queueempty
# The unchanged graph is inside the ruleset-only scope and reaches the
# deliberately unavailable activation phase.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_not_implemented active_generation=1 unchanged=6 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: unchanged YAML candidate did not reach the activation gate: $reload_status"
	error_exit 1
fi

# Native YAML reaches the same private materializer for a ruleset expression
# change. The active generation remains untouched until activation exists.
sed 's/contains "msgnum"/contains "never-match"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_not_implemented active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML ruleset materialization did not reach the activation boundary: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i1
wait_queueempty

# A function expression is normalized as a ruleset-only change but is outside
# B1 lowering. It must fail atomically at Prepare, not leak a partial plan.
sed 's/$msg contains "never-match"/tolower($msg) == "never-match"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported YAML function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi

# Changing the named action is outside the first live-ruleset scope and must
# be classified before any prepare or activation work occurs.
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
	echo "FAIL: unexpected reload status: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i2
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
