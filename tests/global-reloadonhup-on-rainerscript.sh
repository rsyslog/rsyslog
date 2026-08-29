#!/bin/bash
# Verify RainerScript activation and its fail-closed boundaries. Effective
# imtcp flow-control/notification changes and an eligible named-ruleset update
# publish atomically while one TCP session stays open. HUP completion plus
# exact generation/output checks prove the cutover without timing assumptions.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="on")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="prepared" config.enabled="on")
ruleset(name="prepared") {
    if $msg contains "msgnum" then
        action(type="omfile" name="prepared_sink" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
if ! printf '<167>Mar 10 01:00:00 host app: msgnum:00000000\n' >&9; then error_exit 1; fi
wait_content 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=9 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: unchanged on candidate unexpectedly advanced activation: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=reuse"* ]]; then
	echo "FAIL: unchanged RainerScript imtcp profile was not classified reusable: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.base"

# Explicit and omitted module defaults must lower to the same effective
# profile in RainerScript just as they do in YAML. The raw module node changes,
# so the source baseline advances while the existing runtime objects are reused.
sed 's|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on")|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on" flowControl="on")|' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: explicit RainerScript imtcp default did not publish a reusable source baseline: $reload_status"
	error_exit 1
fi
# Repeating the equivalent source must now compare against the newly published
# baseline and stay report-only without another generation bump.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2 unchanged=9 added=0 removed=0 modified=0 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: reusable RainerScript source baseline was not retained: $reload_status"
	error_exit 1
fi

# Real module-default changes reach every input that does not override them.
# The input event-loop fence makes flow control and connection notifications
# atomic while retaining the established session.
sed 's|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on")|module(load="../plugins/imtcp/.libs/imtcp" config.enabled="on" flowControl="off" notifyOnConnectionOpen="on" notifyOnConnectionClose="on")|' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=live_swap"* ]]; then
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
if [[ "$reload_status" != *"result=activated active_generation=4 unchanged=8 added=0 removed=0 modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: restored RainerScript imtcp module profile was not activated: $reload_status"
	error_exit 1
fi

# The imtcp source lowerer must parse a valid changed input through the same
# descriptor/default path as startup. The existing listener/session remain
# live while the session-local flow-control snapshot is updated.
sed 's/ruleset="prepared" config.enabled="on")/ruleset="prepared" config.enabled="on" flowControl="off")/' \
	"$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=5 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: valid imtcp candidate did not activate: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: changed RainerScript imtcp profile was not classified live: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE.base" "$CONF_FILE"

# A ruleset expression plus the restored input profile must pass private
# materialization and the coordinated input-fence/queue-barrier activation.
sed 's/contains "msgnum"/contains "cutover-ack"/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=6 unchanged=7 added=0 removed=0 modified=2 invalid=0"* ]]; then
	echo "FAIL: coordinated imtcp/ruleset materialization did not activate: $reload_status"
	error_exit 1
fi
# The newly published graph is the next comparison baseline. Repeating the
# same candidate must be a no-op and must not advance the generation.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=6 unchanged=9 added=0 removed=0 modified=0 invalid=0"* ]]; then
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
if [[ "$reload_status" != *"result=activated active_generation=7 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
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
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=7 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: optimizer-eliminated action was not rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000003\n' >&9
wait_queueempty

# A function remains deliberately outside B1 lowering. Although the graph
# change is ruleset-only, Prepare must reject it without publishing or leaking
# the already partially cloned plan.
sed 's/if 0 then/if tolower($msg) == "never-match" then/' "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=7 unchanged=8 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unsupported function was not rejected by private Prepare: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: msgnum:00000004\n' >&9
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
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
