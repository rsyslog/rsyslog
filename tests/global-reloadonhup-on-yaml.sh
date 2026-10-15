#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The unnamed mode rejects optimizer action removal, then activates a change
# to another ruleset while preserving the untouched unnamed action's delivery.
# Verify native YAML activation: persistent TCP sessions span the coordinated
# imtcp live/new-session profile and ruleset cutover. Exact HUP generation,
# mixed-capability classification preserves both policy dimensions;
# status, and output checks prove the old/new rules plus later fail-closed
# rejections.
# Later-accept multiline framing is proved by a combined continuation record;
# the held pre-HUP stream remains independently LF-delimited.
# Local/mixed controls reject policy and ruleset-body changes without
# advancing the generation; old-route delivery proves runtime retention.
# The normal path adds one unsupported-global request and an on-to-validate
# commit followed by validate-only HUP: fourteen acknowledged HUPs total.
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
    sed -i "/^rulesets:/a\\  - name: rejected_target\\n    actions:\\n      - type: omfile\\n        name: rejected_sink\\n        template: local_reload_fmt\\n        file: \"$RSYSLOG_OUT_LOG.rejected\"" "$CONF_FILE"
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
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged="* ]]; then
	echo "FAIL: unchanged YAML candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.base"

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


if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    # Input-only target changes must hit the coordinator's local/mixed gate,
    # even with unchanged ruleset bodies. Persistent-stream delivery to the
    # old output proves the cached routing policy was not published.
    for stimulus in policy body; do
        if [[ "$stimulus" == policy ]]; then
            sed 's/ruleset: main/ruleset: rejected_target/' "$CONF_FILE.base" >"$CONF_FILE"
            record=1
        else
            sed 's/contains "msgnum"/contains "never-match"/' "$CONF_FILE.base" >"$CONF_FILE"
            record=2
        fi
        issue_HUP
        reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
        if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
            echo "FAIL: local/mixed YAML $stimulus reload was not rejected: $reload_status"
            error_exit 1
        fi
        printf '<167>Mar 10 01:00:00 host app: msgnum:0000000%s\n' "$record" >&9 || error_exit 1
        wait_content "msgnum:0000000$record" "$RSYSLOG_OUT_LOG"
    done
    exec 9>&-
    exec 8>&-
    shutdown_when_empty
    wait_shutdown
    content_check 'msgnum:00000000'
    content_check 'msgnum:00000001'
    content_check 'msgnum:00000002'
    content_check 'reload unsupported for active local-queue graph, including mixed boundaries'
    if [[ -s "$RSYSLOG_OUT_LOG.rejected" ]]; then
        echo "FAIL: rejected YAML input policy routed messages to the candidate target"
        error_exit 1
    fi
    exit_test
fi

# Making the inherited default explicit changes the source graph but not either
# effective listener profile. Publish that source baseline without pausing the
# two established sessions or replacing runtime objects.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    flowControl: "on"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: equivalent YAML module default did not publish a reusable source baseline: $reload_status"
	error_exit 1
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=9 added=0 removed=0 modified=0 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reusable YAML source baseline was not retained: $reload_status"
	error_exit 1
fi

# Module defaults use the same YAML lifecycle path as input overrides. Toggle
# flow control and connection notifications while retaining both established
# TCP sessions; preserveCase, keepalive, and scalar framing apply to later accepts.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    flowControl: "off"\
    notifyOnConnectionOpen: "on"\
    notifyOnConnectionClose: "on"\
    preserveCase: "off"\
    keepAlive: "on"\
    keepAlive.probes: "3"\
    keepAlive.time: "30"\
    keepAlive.interval: "2"\
    maxFrameSize: "210000"\
    octetCountedFraming: "off"\
    compression.maxExpansionRatio: "2048"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=live_and_new_sessions"* ]]; then
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
if [[ "$reload_status" != *"result=activated active_generation=4 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=live_and_new_sessions"* ]]; then
	echo "FAIL: YAML module-level flow-control restore did not activate: $reload_status"
	error_exit 1
fi

# Native YAML reaches the same materializer and imtcp source lowerer. The
# existing session is fenced while flow control, the later-accept multiline
# profile, and the root are published as one generation.
sed -e 's/contains "msgnum"/contains "cutover-ack"/' \
	-e '/name: first/a\    flowControl: "off"\
    multiline: "on"' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5 unchanged=7 added=0 removed=0 modified=2 invalid=0 source_capability=live_and_new_sessions"* ]]; then
	echo "FAIL: coordinated YAML imtcp/ruleset activation did not publish generation five: $reload_status"
	error_exit 1
fi
# The new accept profile must actually combine continuation lines, while
# the pre-HUP stream still submits a single LF-delimited frame immediately.
# Default parser escaping renders the embedded newline as #012 in one record.
printf '<167>Mar 10 01:00:00 host app: cutover-ack:multiline-old\n' >&9 || error_exit 1
wait_content 'cutover-ack:multiline-old' "$RSYSLOG_OUT_LOG"
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: cutover-ack:multiline-new\ncontinued-body\n<167>Mar 10 01:00:00 host app: cutover-ack:multiline-flush\n' >&7 || error_exit 1
wait_content 'cutover-ack:multiline-new#012continued-body' "$RSYSLOG_OUT_LOG"
exec 7>&-

# The graph publication is part of the same commit boundary as the root swap.
# Repeating the new file must therefore be a no-op against generation five.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=5 unchanged=9 added=0 removed=0 modified=0 invalid=0"* ]]; then
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
if [[ "$reload_status" != *"result=activated active_generation=6 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimized YAML array comparison did not activate: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.active-generation-six"
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
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=6 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimizer-eliminated YAML action was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000003\n' >&9
wait_content 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
wait_queueempty

# A function expression is normalized as a ruleset-only change but is outside
# B1 lowering. It must fail atomically at Prepare, not leak a partial plan.
sed 's/if: '\''0'\''/if: '\''tolower($msg) == "never-match"'\''/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=6 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
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
wait_content 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
wait_queueempty

# The global-node authorization is parameter-specific. Changing a second
# global value must fail closed and retain both generation and TCP sessions.
sed 's/debug.abortOnProgramError: "on"/debug.abortOnProgramError: "off"/' "$CONF_FILE.active-generation-six" \
	>"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=6 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unrelated YAML global change escaped the narrow base gate: $reload_status"
	error_exit 1
fi

# Native YAML uses the same private base profile. Restore generation six,
# change only config.reloadOnHUP, then use the next HUP as the runtime-policy
# oracle: it must validate without publishing another generation.
sed 's/config.reloadOnHUP: "on"/config.reloadOnHUP: "validate"/' "$CONF_FILE.active-generation-six" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=7 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: YAML reload mode base profile was not activated: $reload_status"
	error_exit 1
fi
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=7 unchanged=9 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: activated YAML validate mode did not govern the next HUP: $reload_status"
	error_exit 1
fi
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
awk '/origin=core.reload / { last = $0 } END { print last }' \
	"${RSYSLOG_DYNNAME}.policy.log" > "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_on_total=13 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_on_rejected_total=4 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_validate_total=1 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_legacy_hook_total=14 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
content_check 'reload_capability_rejected_total=4 ' "${RSYSLOG_DYNNAME}.terminal-policy.log"
exit_test
