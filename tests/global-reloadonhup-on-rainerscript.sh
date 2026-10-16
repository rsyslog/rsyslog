#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify RainerScript reloadOnHUP=on parses the candidate without side effects,
# then fails closed because activation is not implemented in this step. The
# internal structured record is the deterministic oracle.
# Renaming the populated output proves the legacy reopen hook still runs.
# Shutdown debug logging must run from normal main-thread context.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on"
       debug.onShutdown="on" debug.logFile="'$RSYSLOG_DYNNAME'.debug.log")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg 0 1
wait_queueempty
mv "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_not_implemented active_generation=1"* ]]; then
	echo "FAIL: unchanged on candidate did not reach the activation gate: $reload_status"
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
injectmsg 1 1
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG2_OUT_LOG"
content_check 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'shadow_reload event=request result=rejected mode=on'
content_check 'rejected_mode=on rejected_reason=candidate_scope_unsupported'
content_check 'reload_on_total=2'
content_check 'reload_on_rejected_total=2'
content_check 'reload_capability_rejected_total=1'
content_check 'reload_legacy_hook_total=2'
assert_content_missing 'result=validated'
content_check 'termination requested by signal' "$RSYSLOG_DYNNAME.debug.log"
exit_test
