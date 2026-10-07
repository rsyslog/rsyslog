#!/bin/bash
# Dynamic port-0 additions and endpoint replacements remain restart-required;
# fixed numeric plain-ptcp additions and named dynamic-listener retirement are
# covered separately. A candidate that combines a named port-0 removal with an
# unsupported port-0 addition must reject atomically: completed HUP/status,
# unchanged generation, absent candidate port file, and old-session/new-old-
# listener messages prove no partial retirement. The fixed-numeric bind
# conflict below must reach prepare in on mode and fail without changing the
# baseline.
# Restoring startup config proves the accepted baseline did not advance; a
# retained-listener profile update must still reload. No sleep is an oracle.
# Also run with RSYSLOG_RELOAD_ENDPOINT_MODE=validate: report-only mode must
# classify restart_required without activation.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
MODE="${RSYSLOG_RELOAD_ENDPOINT_MODE:-on}"
case "$MODE" in on|validate) ;; *) error_exit 1 ;; esac
require_yaml_support
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "'$MODE'"' "$TESTCONF_NM.yaml"
add_yaml_conf '
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"
    name: first
    ruleset: main
  # seed endpoint
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.seed_port"
    name: seed
    ruleset: main
rulesets:
  - name: main
    actions:
      - type: omfile
        name: sink
        file: "'$RSYSLOG_OUT_LOG'"
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
	[[ ! -e "$RSYSLOG_DYNNAME.candidate_port" ]] || error_exit 1
	assert_old_state "$1"
}
awk -v candidate="$RSYSLOG_DYNNAME.candidate_port" '
	/^rulesets:/ {
		print "  - type: imtcp"
		print "    port: \"0\""
		print "    listenPortFileName: \"" candidate "\""
		print "    name: added"
		print "    ruleset: main"
	}
	{ print }
	' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
assert_endpoint_rejected addition

awk -v candidate="$RSYSLOG_DYNNAME.candidate_port" '
	/^  # seed endpoint$/ { skipping = 1 }
	/^rulesets:/ {
		skipping = 0
		print "  - type: imtcp"
		print "    port: \"0\""
		print "    listenPortFileName: \"" candidate "\""
		print "    name: added"
		print "    ruleset: main"
	}
	!skipping { print }
	' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
assert_endpoint_rejected removal-plus-unsupported-addition

# This fixed numeric plain-ptcp addition is structurally supported, so the
# already-bound imdiag port must fail resource preparation in on mode. Validate
# mode reports support only and does not attempt the conflicting bind.
awk -v port="$IMDIAG_PORT" '
	/^rulesets:/ {
		print "  - type: imtcp"
		print "    address: \"127.0.0.1\""
		print "    port: \"" port "\""
		print "    name: conflict"
		print "    ruleset: main"
	}
	{ print }
	' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
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

sed '/    name: first/i\    flowControl: "off"' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
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
exit_test
