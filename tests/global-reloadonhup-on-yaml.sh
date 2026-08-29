#!/bin/bash
# Verify native YAML activation: persistent TCP sessions span the coordinated
# imtcp live/new-session profile and ruleset cutover. Exact HUP generation,
# status, and output checks prove the old/new rules plus later fail-closed
# rejections.
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
add_yaml_conf '    name: first'
add_yaml_conf '    ruleset: main'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port2"'
add_yaml_conf '    name: second'
add_yaml_conf '    ruleset: main'
add_yaml_conf '    flowControl: "on"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    statements:'
add_yaml_conf '      - if: '\''$msg contains "msgnum"'\'''
add_yaml_conf '        action:'
add_yaml_conf '          type: omfile'
add_yaml_conf '          name: sink'
add_yaml_conf '          file: "'$RSYSLOG_OUT_LOG'"'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT2"
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000000-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000000-second\n' >&8; then error_exit 1; fi
wait_content 'msgnum:00000000-first' "$RSYSLOG_OUT_LOG"
wait_content 'msgnum:00000000-second' "$RSYSLOG_OUT_LOG"
# A valid no-op remains report-only and must not advance the generation.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=7 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: unchanged YAML candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.base"

# Making the inherited default explicit changes the source graph but not either
# effective listener profile. Publish that source baseline without pausing the
# two established sessions or replacing runtime objects.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    flowControl: "on"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=6 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: equivalent YAML module default did not publish a reusable source baseline: $reload_status"
	error_exit 1
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=7 added=0 removed=0 modified=0 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reusable YAML source baseline was not retained: $reload_status"
	error_exit 1
fi

# Module defaults use the same YAML lifecycle path as input overrides. Toggle
# flow control and connection notifications while retaining both established
# TCP sessions; preserveCase applies to connections accepted afterwards.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    flowControl: "off"\
    notifyOnConnectionOpen: "on"\
    notifyOnConnectionClose: "on"\
    preserveCase: "off"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=6 added=0 removed=0 modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: YAML module-level flow-control update did not activate: $reload_status"
	error_exit 1
fi
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:notification-live\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:notification-new-session\n' >&7; then error_exit 1; fi
wait_content 'msgnum:notification-live' "$RSYSLOG_OUT_LOG"
wait_content 'msgnum:notification-new-session' "$RSYSLOG_OUT_LOG"
exec 7>&-
cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4 unchanged=6 added=0 removed=0 modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: YAML module-level flow-control restore did not activate: $reload_status"
	error_exit 1
fi

# Native YAML reaches the same materializer and imtcp source lowerer. The
# existing session is fenced while both the new flow-control snapshot and root
# are published as one generation.
sed -e 's/contains "msgnum"/contains "cutover-ack"/' \
	-e '/name: first/a\    flowControl: "off"' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5 unchanged=5 added=0 removed=0 modified=2 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: coordinated YAML imtcp/ruleset activation did not publish generation five: $reload_status"
	error_exit 1
fi
# The graph publication is part of the same commit boundary as the root swap.
# Repeating the new file must therefore be a no-op against generation five.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=5 unchanged=7 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: activated YAML graph was not retained as generation-five baseline: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000001\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: cutover-ack-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: cutover-ack-second\n' >&8; then error_exit 1; fi
wait_content 'cutover-ack-first' "$RSYSLOG_OUT_LOG"
wait_content 'cutover-ack-second' "$RSYSLOG_OUT_LOG"

# Exercise the opposite array canonicalization shape from the RainerScript
# test: an unsorted RHS must be sorted before the runtime bsearch evaluator is
# allowed to execute the private plan.
sed 's/$msg contains "cutover-ack"/"match_me" == ["z-last", "match_me", "a-first"]/' "$CONF_FILE" \
	>"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=6 unchanged=6 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimized YAML array comparison did not activate: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000002-first\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000002-second\n' >&8; then error_exit 1; fi
wait_content 'msgnum:00000002-first' "$RSYSLOG_OUT_LOG"
wait_content 'msgnum:00000002-second' "$RSYSLOG_OUT_LOG"

# A constant-false candidate would make the optimizer remove the unchanged
# named action. Reject until action queues have independent generation
# ownership; the active array-matching plan must remain live.
sed 's/"match_me" == \["z-last", "match_me", "a-first"\]/0/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=6 unchanged=6 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimizer-eliminated YAML action was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000003\n' >&9
wait_queueempty

# A function expression is normalized as a ruleset-only change but is outside
# B1 lowering. It must fail atomically at Prepare, not leak a partial plan.
sed 's/if: '\''0'\''/if: '\''tolower($msg) == "never-match"'\''/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=6 unchanged=6 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported YAML function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi

# Changing the named action is outside the first live-ruleset scope and must
# be classified before any prepare or activation work occurs.
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=6"* ]]; then
	echo "FAIL: unexpected reload status: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000004\n' >&9
wait_queueempty
exec 9>&-
exec 8>&-
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000-first' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:notification-live' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:notification-new-session' "$RSYSLOG_OUT_LOG"
content_check 'cutover-ack-first' "$RSYSLOG_OUT_LOG"
content_check 'cutover-ack-second' "$RSYSLOG_OUT_LOG"
custom_assert_content_missing 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002-first' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002-second' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
