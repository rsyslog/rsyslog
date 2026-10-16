#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify native YAML activation: the first record proves the old ruleset,
# the second proves the atomically swapped root, and an action-change
# rejection proves the active root remains unchanged. HUP completion and exact
# generation/status snapshots are the deterministic control-plane oracle.
# RSYSLOG_RELOAD_LOCAL_CONTROL also runs local/mixed rejection controls: both
# records must reach the old root, with generation one retained after HUP.
# Pure-global terminal impstats must account for all seven acknowledged HUPs;
# local/mixed controls omit impstats, which is outside the local input contract.
# The unnamed control requires rejection before optimizer removal can retire
# an action; unchanged generation and post-rejection delivery prove retention.
# Updating only another ruleset must still activate and preserve that delivery.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
if [[ -z "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    require_plugin impstats
fi
generate_conf --yaml-only
CONF_FILE="${TESTCONF_NM}.yaml"
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    RSYSLOG_OUT_LOG="$PWD/$RSYSLOG_OUT_LOG"
fi
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
if [[ -z "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    add_yaml_conf '  - load: "../plugins/impstats/.libs/impstats"'
    add_yaml_conf '    interval: "1"'
    add_yaml_conf '    log.syslog: "off"'
    add_yaml_conf '    log.file: "'$RSYSLOG_DYNNAME'.policy.log"'
fi
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
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    sed -i '/^rulesets:/i\templates:\n  - name: local_reload_fmt\n    type: string\n    string: "%msg%\\n"' "$CONF_FILE"
    sed -i '/          type: omfile/a\          template: local_reload_fmt' "$CONF_FILE"
    # Add a default-ruleset diagnostic sink as test instrumentation; all tested
    # queue and candidate ruleset objects remain native YAML.
    printf '# SPDX-License-Identifier: Apache-2.0\naction(type="omfile" file="%s" template="local_reload_fmt")\n' \
        "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.default.conf"
    add_yaml_conf 'include:'
    add_yaml_conf '  - path: "'$RSYSLOG_DYNNAME'.default.conf"'
    add_yaml_conf 'main_queue:'
    add_yaml_conf '  queue.type: FixedArray'
    add_yaml_conf '  queue.scope: local'
    add_yaml_conf '  queue.size: "1000"'
    add_yaml_conf '  queue.local.frontendSize: "100"'
    add_yaml_conf '  queue.local.maxFrontends: "8"'
    if [[ "$RSYSLOG_RELOAD_LOCAL_CONTROL" == local ]]; then
        sed -i '/  - name: main/a\    queue.type: FixedArray\n    queue.scope: local\n    queue.size: "1000"\n    queue.local.frontendSize: "100"\n    queue.local.maxFrontends: "8"' "$CONF_FILE"
    else
        sed -i '/  - name: main/a\    queue.type: FixedArray\n    queue.scope: global\n    queue.size: "1000"' "$CONF_FILE"
    fi
fi
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    ../tools/rsyslogd -N1 -f "$CONF_FILE" -M../runtime/.libs || error_exit 1
fi
if [[ -n "${RSYSLOG_RELOAD_UNNAMED_CONTROL:-}" ]]; then
    sed -i '/          name: sink/d' "$CONF_FILE"
    add_yaml_conf '  - name: control'
    add_yaml_conf '    script: '\''if $msg contains "control-before" then stop'\'''
fi
startup
tcpflood -m1 -i0
wait_queueempty
# A valid no-op remains report-only and must not advance the generation.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged="* ]]; then
	echo "FAIL: unchanged YAML candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi

# Native YAML reaches the same materializer and publishes the candidate graph
# only with the root swap.
if [[ -n "${RSYSLOG_RELOAD_UNNAMED_CONTROL:-}" ]]; then
    sed -i 's/$msg contains "msgnum"/0/' "$CONF_FILE"
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
        echo "FAIL: optimizer-eliminated unnamed YAML action was not rejected: $reload_status"
        error_exit 1
    fi
    tcpflood -m1 -i1
    wait_queueempty
    # Native YAML parity: an untouched unnamed action remains independent.
    sed -i 's/if: '\''0'\''/if: '\''$msg contains "msgnum"'\''/; s/control-before/control-after/' "$CONF_FILE"
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"result=activated active_generation=2"* ]]; then
        echo "FAIL: untouched unnamed YAML action blocked another ruleset: $reload_status"
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
        echo "FAIL: local/mixed YAML reload was not rejected: $reload_status"
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
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=7 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: YAML ruleset activation did not publish generation two: $reload_status"
	error_exit 1
fi
# The graph publication is part of the same commit boundary as the root swap.
# Repeating the new file must therefore be a no-op against generation two.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=8 added=0 removed=0 modified=0 invalid=0"* ]]; then
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
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=7 added=0 removed=0 modified=1 invalid=0"* ]]; then
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
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=7 added=0 removed=0 modified=1 invalid=0"* ]]; then
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
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=3 unchanged=7 added=0 removed=0 modified=1 invalid=0"* ]]; then
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
custom_assert_content_missing 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
awk '/origin=core.reload / { last = $0 } END { print last }' \
	"${RSYSLOG_DYNNAME}.policy.log" > "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_on_total=7 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_on_rejected_total=3 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_legacy_hook_total=7 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_capability_rejected_total=3 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
exit_test
