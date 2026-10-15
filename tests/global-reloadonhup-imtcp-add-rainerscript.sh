#!/bin/bash
# Dynamic port-0 additions and endpoint replacements remain restart-required;
# fixed numeric plain-ptcp additions and named dynamic-listener retirement are
# covered separately. A candidate that combines a named port-0 removal with an
# unsupported port-0 addition must reject atomically: completed HUP/status,
# unchanged generation, absent candidate port file, and old-session/new-old-
# listener messages prove no partial retirement. The fixed-numeric bind
# conflict below must reach prepare in on mode and fail without changing the
# baseline.
# Each unsupported candidate must identify its offending second input in the
# configured .started omfile: both an explicit name and the
# default "imtcp" name are checked, including escaping of quote, backslash,
# and newline bytes. The diagnostic assertions run after synchronized shutdown;
# HUP status and old-listener delivery are checked before that.
# Restoring startup config proves the accepted baseline did not advance; a
# retained-listener profile update must still reload. No sleep is an oracle.
# Also run with RSYSLOG_RELOAD_ENDPOINT_MODE=validate: report-only mode must
# classify restart_required without activation.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
MODE="${RSYSLOG_RELOAD_ENDPOINT_MODE:-on}"
case "$MODE" in on|validate) ;; *) error_exit 1 ;; esac
generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="'$MODE'")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="first" ruleset="main")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.seed_port" name="seed" ruleset="main")
ruleset(name="main") {
  action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
wait_file_exists "$RSYSLOG_DYNNAME.seed_port"
SEED_PORT="$(<"$RSYSLOG_DYNNAME.seed_port")"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
exec 8<>"/dev/tcp/127.0.0.1/$SEED_PORT"
cp "$CONF_FILE" "$CONF_FILE.startup"

assert_old_state() {
	local tag="$1"
	printf '<167>Mar 10 01:00:00 host app: retained-first-%s\n' "$tag" >&9 || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: retained-seed-%s\n' "$tag" >&8 || error_exit 1
	wait_content "retained-first-$tag" "$RSYSLOG_OUT_LOG"
	wait_content "retained-seed-$tag" "$RSYSLOG_OUT_LOG"
	exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: retained-accept-%s\n' "$tag" >&7 || error_exit 1
	exec 7>&-
	wait_content "retained-accept-$tag" "$RSYSLOG_OUT_LOG"
	exec 7<>"/dev/tcp/127.0.0.1/$SEED_PORT" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: seed-accept-%s\n' "$tag" >&7 || error_exit 1
	exec 7>&-
	wait_content "seed-accept-$tag" "$RSYSLOG_OUT_LOG"
}

assert_endpoint_rejected() {
	issue_HUP
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	expected_result=candidate_scope_unsupported
	[[ "$MODE" == validate ]] && expected_result=reported_only
	if [[ "$reload_status" != *"result=$expected_result active_generation=1"* ||
	      "$reload_status" != *"source_capability=restart_required"* ||
	      "$reload_status" != *"retirement_pending=0"* ]]; then
		echo "FAIL: candidate escaped existing-listener scope: $reload_status"
		error_exit 1
	fi
	[[ -z "$2" || ! -e "$2" ]] || error_exit 1
	assert_old_state "$1"
}
sed '/name="first"/a input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.candidate_port" name="added" ruleset="main")' \
	"$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
assert_endpoint_rejected addition "$RSYSLOG_DYNNAME.candidate_port"

# Removing the second input's configured name forces positional classification.
# Its default imtcp identity must still be reported as candidate input #2.
sed 's/ name="seed"//' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
assert_endpoint_rejected anonymous-second-input ""

# Config names are untrusted diagnostic text. These RainerScript escapes
# decode to a quote, backslash, and newline in the configured name.
awk -v portfile="$RSYSLOG_DYNNAME.malicious_port" '
	/name="first"/ {
		print
		print "input(type=\"imtcp\" port=\"0\" listenPortFileName=\"" portfile "\" name=\"bad\\\"\\\\\\nInjected\" ruleset=\"main\")"
		next
	}
	{ print }
	' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
assert_endpoint_rejected malicious-name "$RSYSLOG_DYNNAME.malicious_port"

sed -e '/name="seed"/d' \
	-e '/name="first"/a input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.candidate_port" name="added" ruleset="main")' \
	"$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
assert_endpoint_rejected removal-plus-unsupported-addition "$RSYSLOG_DYNNAME.candidate_port"

# This fixed numeric plain-ptcp addition is structurally supported, so the
# already-bound imdiag port must fail resource preparation in on mode. Validate
# mode reports support only and does not attempt the conflicting bind.
sed '/name="first"/a input(type="imtcp" address="127.0.0.1" port="'$IMDIAG_PORT'" name="conflict" ruleset="main")' \
	"$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
expected_result=activation_failed
[[ "$MODE" == validate ]] && expected_result=reported_only
if [[ "$reload_status" != *"result=$expected_result active_generation=1"* ||
      "$reload_status" != *"added=1 removed=0 modified=0 invalid=0"* ||
      "$reload_status" != *"source_capability=new_sessions"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: numeric plain-ptcp bind conflict did not preserve the accepted generation: $reload_status"
	error_exit 1
fi
assert_old_state bind-conflict

cp "$CONF_FILE.startup" "$CONF_FILE" || error_exit 1
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1"* ||
      "$reload_status" != *"added=0 removed=0 modified=0 invalid=0"* ||
      "$reload_status" != *"source_capability=reuse"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: rejection changed accepted baseline: $reload_status"
	error_exit 1
fi

sed 's/name="first"/flowControl="off" name="first"/' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
expected_result=activated
expected_generation=2
if [[ "$MODE" == validate ]]; then
	expected_result=reported_only
	expected_generation=1
fi
if [[ "$reload_status" != *"result=$expected_result active_generation=$expected_generation"* ||
      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: supported retained-listener profile rejected: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: retained-profile-first\n' >&9 || error_exit 1
printf '<167>Mar 10 01:00:00 host app: retained-profile-seed\n' >&8 || error_exit 1
wait_content 'retained-profile-first' "$RSYSLOG_OUT_LOG"
wait_content 'retained-profile-seed' "$RSYSLOG_OUT_LOG"
exec 8>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
content_count_check 'imtcp: reload requires restart for candidate input #2 name="added": unsupported addition' 2 \
	"$RSYSLOG_DYNNAME.started"
content_count_check 'imtcp: reload requires restart for candidate input #2 name="imtcp": unsupported change' 1 \
	"$RSYSLOG_DYNNAME.started"
content_count_check 'imtcp: reload requires restart for candidate input #2 name="bad\x22\x5c\x0aInjected": unsupported addition' 1 \
	"$RSYSLOG_DYNNAME.started"
exit_test
