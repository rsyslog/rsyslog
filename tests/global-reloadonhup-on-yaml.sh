#!/bin/bash
# Verify native YAML activation: the first record proves the old ruleset,
# the second proves the atomically swapped root, and an action-change
# rejection proves the active root remains unchanged. HUP completion and exact
# generation/status snapshots are the deterministic control-plane oracle.
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
# A valid no-op remains report-only and must not advance the generation.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=6 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: unchanged YAML candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi

# Native YAML reaches the same materializer and publishes the candidate graph
# only with the root swap.
sed 's/contains "msgnum"/contains "never-match"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML ruleset activation did not publish generation two: $reload_status"
	error_exit 1
fi
# The graph publication is part of the same commit boundary as the root swap.
# Repeating the new file must therefore be a no-op against generation two.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=6 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: activated YAML graph was not retained as baseline: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i1
wait_queueempty

# Exercise the opposite array canonicalization shape from the RainerScript
# test: an unsorted RHS must be sorted before the runtime bsearch evaluator is
# allowed to execute the private plan.
sed 's/$msg contains "never-match"/"match_me" == ["z-last", "match_me", "a-first"]/' "$CONF_FILE" \
	>"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimized YAML array comparison did not activate: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i2
wait_queueempty

# A constant-false candidate would make the optimizer remove the unchanged
# named action. Reject until action queues have independent generation
# ownership; the active array-matching plan must remain live.
sed 's/"match_me" == \["z-last", "match_me", "a-first"\]/0/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimizer-eliminated YAML action was not rejected: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i3
wait_queueempty

# A function expression is normalized as a ruleset-only change but is outside
# B1 lowering. It must fail atomically at Prepare, not leak a partial plan.
sed 's/if: '\''0'\''/if: '\''tolower($msg) == "never-match"'\''/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported YAML function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi

# Changing the named action is outside the first live-ruleset scope and must
# be classified before any prepare or activation work occurs.
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3"* ]]; then
	echo "FAIL: unexpected reload status: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i4
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
assert_content_missing 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
