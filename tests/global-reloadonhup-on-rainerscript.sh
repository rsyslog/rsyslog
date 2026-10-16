#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify RainerScript activation and its fail-closed boundaries. The no-op
# remains generation one, the eligible named-ruleset update atomically
# publishes generation two, and later rejected candidates retain it.
# RSYSLOG_RELOAD_LOCAL_CONTROL selects local or mixed graph rejection controls;
# the unchanged generation and delivery after rejection prove root retention.
# The unnamed control rejects optimizer removal of an active unnamed action;
# synchronized status and a second delivered record prove the old root survives.
# A subsequent other-ruleset update must activate with that unnamed root intact.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
CONF_FILE="${TESTCONF_NM}.conf"
# Use a separate output file so fixture edits work with both BSD and GNU sed.
edit_reload_conf() {
    sed "$@" "$CONF_FILE" >"$CONF_FILE.tmp" &&
        mv "$CONF_FILE.tmp" "$CONF_FILE" || error_exit 1
}
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    RSYSLOG_OUT_LOG="$PWD/$RSYSLOG_OUT_LOG"
fi
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on"
       debug.onShutdown="on" debug.logFile="'$RSYSLOG_DYNNAME'.debug.log")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="prepared")
ruleset(name="prepared") {
    if $msg contains "msgnum" then
        action(type="omfile" name="prepared_sink" file="'$RSYSLOG_OUT_LOG'")
}
'
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    edit_reload_conf '1i\
template(name="local_reload_fmt" type="string" string="%msg%\\n")
'
    edit_reload_conf 's/type="omfile"/type="omfile" template="local_reload_fmt"/'
    edit_reload_conf "s|file=\"./|file=\"$PWD/|g"
    add_conf 'main_queue(queue.type="FixedArray" queue.scope="local" queue.size="1000" queue.local.frontendSize="100" queue.local.maxFrontends="8")'
    if [[ "$RSYSLOG_RELOAD_LOCAL_CONTROL" == local ]]; then
        edit_reload_conf 's/ruleset(name="prepared")/ruleset(name="prepared" queue.type="FixedArray" queue.scope="local" queue.size="1000" queue.local.frontendSize="100" queue.local.maxFrontends="8")/'
    else
        edit_reload_conf 's/ruleset(name="prepared")/ruleset(name="prepared" queue.type="FixedArray" queue.scope="global" queue.size="1000")/'
    fi
fi
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    ../tools/rsyslogd -N1 -f "$CONF_FILE" -M../runtime/.libs || error_exit 1
fi
if [[ -n "${RSYSLOG_RELOAD_UNNAMED_CONTROL:-}" ]]; then
    edit_reload_conf 's/ name="prepared_sink"//'
    add_conf 'ruleset(name="control") { if $msg contains "control-before" then stop }'
fi
startup
tcpflood -m1 -i0
wait_queueempty
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged="* ]]; then
	echo "FAIL: unchanged on candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi

# A ruleset-only expression change with an unchanged named action must pass
# private materialization and live activation.
if [[ -n "${RSYSLOG_RELOAD_UNNAMED_CONTROL:-}" ]]; then
    edit_reload_conf 's/$msg contains "msgnum"/0/'
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
        echo "FAIL: optimizer-eliminated unnamed action was not rejected: $reload_status"
        error_exit 1
    fi
    tcpflood -m1 -i1
    wait_queueempty
    # The unnamed action must not block a change to another ruleset.
    edit_reload_conf 's/if 0 then/if $msg contains "msgnum" then/; s/control-before/control-after/'
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"result=activated active_generation=2"* ]]; then
        echo "FAIL: untouched unnamed action blocked another ruleset: $reload_status"
        error_exit 1
    fi
    tcpflood -m1 -i2
    wait_queueempty
    shutdown_when_empty
    wait_shutdown
    content_check 'msgnum:00000000'
    content_check 'msgnum:00000001'
    content_check 'msgnum:00000002'
    exit_test
fi
sed 's/contains "msgnum"/contains "never-match"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
        echo "FAIL: local/mixed reload was not rejected: $reload_status"
        error_exit 1
    fi
    tcpflood -m1 -i1
    wait_queueempty
    shutdown_when_empty
    wait_shutdown
    content_check 'msgnum:00000000'
    content_check 'msgnum:00000001'
    content_check 'reload unsupported for active local-queue graph, including mixed boundaries'
    exit_test
fi
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: private ruleset materialization did not activate: $reload_status"
	error_exit 1
fi
# The newly published graph is the next comparison baseline. Repeating the
# same candidate must be a no-op and must not advance the generation.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=10 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: activated RainerScript graph was not retained as baseline: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i1
wait_queueempty

# Runtime array equality depends on startup's optimizer moving an array to the
# right-hand side and sorting it for bsearch(). An intentionally unsorted
# left-hand array therefore proves that private plans receive the same
# canonicalization before publication.
sed 's/$msg contains "never-match"/["z-last", "match_me", "a-first"] == "match_me"/' "$CONF_FILE" \
	>"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimized array comparison did not activate: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i2
wait_queueempty

# Startup optimization would remove this branch and its syntactically
# unchanged action. Until action queues have independent generation ownership,
# that must be rejected rather than retiring the active action implicitly.
sed 's/\["z-last", "match_me", "a-first"\] == "match_me"/0/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimizer-eliminated action was not rejected: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i3
wait_queueempty

# A function remains deliberately outside B1 lowering. Although the graph
# change is ruleset-only, Prepare must reject it without publishing or leaking
# the already partially cloned plan.
sed 's/if 0 then/if tolower($msg) == "never-match" then/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i4
wait_queueempty

# An action change is outside the first ruleset-only activation scope. The
# report gate must distinguish that capability rejection from the later,
# not-yet-implemented activation phase while keeping the old action active.
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3"* ]]; then
	echo "FAIL: action change was not rejected by the ruleset-only scope gate: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i5
wait_queueempty

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
if [[ "$reload_status" != *"result=candidate_report_invalid active_generation=3"* ]]; then
	echo "FAIL: invalid ON report was hidden by the scope gate: $reload_status"
	error_exit 1
fi
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
assert_content_missing 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000005' "$RSYSLOG_OUT_LOG"
content_check 'shadow_reload event=request result=rejected mode=on'
content_check 'rejected_mode=on rejected_reason=candidate_report_invalid'
content_check 'reload_on_total=8'
content_check 'reload_on_rejected_total=4'
content_check 'reload_capability_rejected_total=3'
content_check 'reload_legacy_hook_total=8'
assert_content_missing 'result=validated'
content_check 'termination requested by signal' "$RSYSLOG_DYNNAME.debug.log"
exit_test
