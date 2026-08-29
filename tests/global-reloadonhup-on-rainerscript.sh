#!/bin/bash
# Verify RainerScript reloadOnHUP=on parses candidates without side effects and
# distinguishes the current fail-closed boundaries: unavailable activation,
# unsupported scope, and an invalid normalized report. Structured status and
# counters are the deterministic oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
ruleset(name="prepared") {
    if $msg contains "prepare-before" then
        action(type="omfile" name="prepared_sink" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_not_implemented active_generation=1 unchanged=7 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: unchanged on candidate did not reach the activation gate: $reload_status"
	error_exit 1
fi

# A ruleset-only expression change with an unchanged named action must pass
# private materialization. The deterministic oracle is the later activation
# boundary; a prepare failure would instead report candidate_scope_unsupported.
sed 's/prepare-before/prepare-after/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_not_implemented active_generation=1 unchanged=6 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: private ruleset materialization did not reach the activation boundary: $reload_status"
	error_exit 1
fi

# A function remains deliberately outside B1 lowering. Although the graph
# change is ruleset-only, Prepare must reject it without publishing or leaking
# the already partially cloned plan.
sed 's/$msg contains "prepare-after"/tolower($msg) == "prepare-after"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1 unchanged=6 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi

# An action change is outside the first ruleset-only activation scope. The
# report gate must distinguish that capability rejection from the later,
# not-yet-implemented activation phase while keeping the old action active.
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
	echo "FAIL: action change was not rejected by the ruleset-only scope gate: $reload_status"
	error_exit 1
fi

# An invalid normalized report must keep its report-invalid reason. The ON
# scope gate is intentionally skipped here, so this outcome is not counted as
# a capability rejection.
cat >"$CONF_FILE" <<CONF_EOF
global(config.reloadOnHUP="on")
ruleset(name="first") {
    action(type="omfile" name="duplicate" file="$RSYSLOG2_OUT_LOG")
}
ruleset(name="second") {
    action(type="omfile" name="duplicate" file="$RSYSLOG2_OUT_LOG")
}
CONF_EOF
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_report_invalid active_generation=1"* ]]; then
	echo "FAIL: invalid ON report was hidden by the scope gate: $reload_status"
	error_exit 1
fi
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'shadow_reload event=request result=rejected mode=on'
content_check 'rejected_mode=on rejected_reason=candidate_report_invalid'
content_check 'reload_on_total=5'
content_check 'reload_on_rejected_total=5'
content_check 'reload_capability_rejected_total=2'
content_check 'reload_legacy_hook_total=5'
assert_content_missing 'result=validated'
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
