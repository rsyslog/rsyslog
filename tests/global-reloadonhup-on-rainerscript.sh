#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# The unnamed mode rejects optimizer action removal, then activates a change
# to another ruleset while preserving the untouched unnamed action's delivery.
# Verify RainerScript activation and its fail-closed boundaries. Effective
# imtcp live/new-session profile changes and an eligible named-ruleset update
# publish atomically while one TCP session stays open. HUP completion plus
# exact generation/output checks prove the cutover without timing assumptions.
# Later-accept multiline framing is proved by a combined continuation record;
# the held pre-HUP stream remains independently LF-delimited.
# Local/mixed controls reject policy and ruleset-body changes without
# advancing the generation; old-route delivery proves runtime retention.
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
module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="prepared" config.enabled="on")
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
    add_conf 'ruleset(name="rejected_target") {
        action(type="omfile" name="rejected_sink" template="local_reload_fmt" file="'$RSYSLOG_OUT_LOG'.rejected")
    }'
fi
if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    ../tools/rsyslogd -N1 -f "$CONF_FILE" -M../runtime/.libs || error_exit 1
fi
if [[ -n "${RSYSLOG_RELOAD_UNNAMED_CONTROL:-}" ]]; then
    edit_reload_conf 's/ name="prepared_sink"//'
    add_conf 'ruleset(name="control") { if $msg contains "control-before" then stop }'
fi

startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000000\n' >&9; then error_exit 1; fi
wait_content 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged="* ]]; then
	echo "FAIL: unchanged on candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=reuse"* ]]; then
	echo "FAIL: unchanged RainerScript imtcp profile was not classified reusable: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.base"

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


if [[ -n "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
    # The first candidate changes only an input's target shell, not a ruleset
    # body. Old-route delivery proves that the rejected live policy did not
    # change the accepted session's cached target. Then retain the inherited
    # body-only rejection control against the same unchanged baseline.
    for stimulus in policy body; do
        if [[ "$stimulus" == policy ]]; then
            sed 's/ruleset="prepared" config.enabled/ruleset="rejected_target" config.enabled/' \
                "$CONF_FILE.base" >"$CONF_FILE"
            record=1
        else
            sed 's/contains "msgnum"/contains "never-match"/' "$CONF_FILE.base" >"$CONF_FILE"
            record=2
        fi
        issue_HUP
        reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
        if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1"* ]]; then
            echo "FAIL: local/mixed $stimulus reload was not rejected: $reload_status"
            error_exit 1
        fi
        printf '<167>Mar 10 01:00:00 host app: msgnum:0000000%s\n' "$record" >&9 || error_exit 1
        wait_content "msgnum:0000000$record" "$RSYSLOG_OUT_LOG"
    done
    exec 9>&-
    shutdown_when_empty
    wait_shutdown
    content_check 'msgnum:00000000'
    content_check 'msgnum:00000001'
    content_check 'msgnum:00000002'
    content_check 'reload unsupported for active local-queue graph, including mixed boundaries'
    if [[ -s "$RSYSLOG_OUT_LOG.rejected" ]]; then
        echo "FAIL: rejected input policy routed messages to the candidate target"
        error_exit 1
    fi
    exit_test
fi

if [[ -z "${RSYSLOG_RELOAD_LOCAL_CONTROL:-}" ]]; then
# Explicit and omitted module defaults must lower to the same effective
# profile in RainerScript just as they do in YAML. The raw module node changes,
# so the source baseline advances while the existing runtime objects are reused.
sed 's|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on")|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on" flowControl="on")|' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=9 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: explicit RainerScript imtcp default did not publish a reusable source baseline: $reload_status"
	error_exit 1
fi
# Repeating the equivalent source must now compare against the newly published
# baseline and stay report-only without another generation bump.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=10 added=0 removed=0 modified=0 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reusable RainerScript source baseline was not retained: $reload_status"
	error_exit 1
fi

# Real module-default changes reach every input that does not override them.
# The input event-loop fence makes flow control and connection notifications
# atomic while retaining the established session; preserveCase and TCP
# keepalive and the scalar framing profile change only the accept profile used
# after the fence.
sed 's|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on")|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on" flowControl="off" notifyOnConnectionOpen="on" notifyOnConnectionClose="on" preserveCase="off" keepAlive="on" keepAlive.probes="3" keepAlive.time="30" keepAlive.interval="2" maxFrameSize="210000" octetCountedFraming="off" compression.maxExpansionRatio="2048")|' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=9 added=0 removed=0 modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: changed RainerScript imtcp module profile was not activated: $reload_status"
	error_exit 1
fi
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
wait_content 'imtcp: connection established with host:' "$RSYSLOG_DYNNAME.started"
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:notification-live\n' >&9; then error_exit 1; fi
wait_content 'msgnum:notification-live' "$RSYSLOG_OUT_LOG"
exec 7>&-
wait_content 'closed by remote peer' "$RSYSLOG_DYNNAME.started"

# Restore the omitted/on defaults through the same live path so the next input
# override begins from a published source/runtime baseline.
cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=4 unchanged=9 added=0 removed=0 modified=1 invalid=0 source_capability=new_sessions"* ]]; then
	echo "FAIL: restored RainerScript imtcp module profile was not activated: $reload_status"
	error_exit 1
fi

# The imtcp source lowerer must parse a valid changed input through the same
# descriptor/default path as startup. The existing listener/session remain
# live while flow control updates immediately and multiline changes only the
# profile used for later accepts.
sed 's/ruleset="prepared" config.enabled="on")/ruleset="prepared" config.enabled="on" flowControl="off" multiLine="on")/' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: valid imtcp candidate did not activate: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=new_sessions"* ]]; then
	echo "FAIL: changed RainerScript imtcp profile was not classified for new sessions: $reload_status"
	error_exit 1
fi
# The new accept profile must actually combine continuation lines, while
# the pre-HUP stream still submits a single LF-delimited frame immediately.
# Default parser escaping renders the embedded newline as #012 in one record.
printf '<167>Mar 10 01:00:00 host app: msgnum:multiline-old\n' >&9 || error_exit 1
wait_content 'msgnum:multiline-old' "$RSYSLOG_OUT_LOG"
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: msgnum:multiline-new\ncontinued-body\n<167>Mar 10 01:00:00 host app: msgnum:multiline-flush\n' >&7 || error_exit 1
wait_content 'msgnum:multiline-new#012continued-body' "$RSYSLOG_OUT_LOG"
exec 7>&-

cp "$CONF_FILE.base" "$CONF_FILE"

fi

# A ruleset expression plus the restored input profile must pass private
# materialization and the coordinated input-fence/queue-barrier activation.
sed 's/contains "msgnum"/contains "cutover-ack"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=6 unchanged=8 added=0 removed=0 modified=2 invalid=0"* ]]; then
	echo "FAIL: coordinated imtcp/ruleset materialization did not activate: $reload_status"
	error_exit 1
fi
# The newly published graph is the next comparison baseline. Repeating the
# same candidate must be a no-op and must not advance the generation.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=6 unchanged=10 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: activated RainerScript graph was not retained as baseline: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000001\n' >&9; then error_exit 1; fi
if ! printf '<167>Mar 10 01:00:00 host app: cutover-ack\n' >&9; then error_exit 1; fi
# TCP ordering plus this visible marker proves the preceding rejected record
# was evaluated by generation five before the next HUP.
wait_content 'cutover-ack' "$RSYSLOG_OUT_LOG"

# Runtime array equality depends on startup's optimizer moving an array to the
# right-hand side and sorting it for bsearch(). An intentionally unsorted
# left-hand array therefore proves that private plans receive the same
# canonicalization before publication.
sed 's/$msg contains "cutover-ack"/["z-last", "match_me", "a-first"] == "match_me"/' "$CONF_FILE" \
	>"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=7 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimized array comparison did not activate: $reload_status"
	error_exit 1
fi
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000002\n' >&9; then error_exit 1; fi
wait_content 'msgnum:00000002' "$RSYSLOG_OUT_LOG"

# Startup optimization would remove this branch and its syntactically
# unchanged action. Until action queues have independent generation ownership,
# that must be rejected rather than retiring the active action implicitly.
sed 's/\["z-last", "match_me", "a-first"\] == "match_me"/0/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=7 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimizer-eliminated action was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000003\n' >&9
wait_content 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
wait_queueempty

# A function remains deliberately outside B1 lowering. Although the graph
# change is ruleset-only, Prepare must reject it without publishing or leaking
# the already partially cloned plan.
sed 's/if 0 then/if tolower($msg) == "never-match" then/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=7 unchanged=9 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000004\n' >&9
wait_content 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
wait_queueempty

# An action change is outside the first ruleset-only activation scope. The
# report gate must distinguish that capability rejection from the later,
# not-yet-implemented activation phase while keeping the old action active.
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=7"* ]]; then
	echo "FAIL: action change was not rejected by the ruleset-only scope gate: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000005\n' >&9
wait_content 'msgnum:00000005' "$RSYSLOG_OUT_LOG"
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
if [[ "$reload_status" != *"result=candidate_report_invalid active_generation=7"* ]]; then
	echo "FAIL: invalid ON report was hidden by the scope gate: $reload_status"
	error_exit 1
fi
wait_queueempty
exec 9>&-
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:notification-live' "$RSYSLOG_OUT_LOG"
content_check 'cutover-ack' "$RSYSLOG_OUT_LOG"
assert_content_missing 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000005' "$RSYSLOG_OUT_LOG"
content_check 'shadow_reload event=request result=rejected mode=on'
content_check 'rejected_mode=on rejected_reason=candidate_report_invalid'
content_check 'reload_on_total=13'
content_check 'reload_on_rejected_total=4'
content_check 'reload_capability_rejected_total=3'
content_check 'reload_legacy_hook_total=13'
assert_content_missing 'result=validated'
content_check 'termination requested by signal' "$RSYSLOG_DYNNAME.debug.log"
exit_test
