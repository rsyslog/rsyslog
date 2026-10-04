#!/bin/bash
# Deliberate existing-listener-only milestone: additions, removals and endpoint
# replacements require restart until worker activation readiness is provable.
# Completed HUP/status, unchanged generation, absent candidate port file and
# old-session/new-old-listener messages prove rejection before resource prepare.
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
	printf '<167>Mar 10 01:00:00 host app: retained-first-%s\n' "$1" >&9 || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: retained-seed-%s\n' "$1" >&8 || error_exit 1
	wait_content "retained-first-$1" "$RSYSLOG_OUT_LOG"
	wait_content "retained-seed-$1" "$RSYSLOG_OUT_LOG"
	exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
	printf '<167>Mar 10 01:00:00 host app: retained-accept-%s\n' "$1" >&7 || error_exit 1
	exec 7>&-
	wait_content "retained-accept-$1" "$RSYSLOG_OUT_LOG"
	exec 7<>"/dev/tcp/127.0.0.1/$SEED_PORT"
	printf '<167>Mar 10 01:00:00 host app: seed-accept-%s\n' "$1" >&7 || error_exit 1
	exec 7>&-
	wait_content "seed-accept-$1" "$RSYSLOG_OUT_LOG"
}
# Changing a dynamic endpoint's port-file identity must not privately bind
# a replacement accept socket. Both established and new old sessions survive.
sed 's/\.tcpflood_port"/.candidate_port"/' "$CONF_FILE.startup" >"$CONF_FILE"
assert_endpoint_rejected replacement

cp "$CONF_FILE.startup" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1"* ||
      "$reload_status" != *"added=0 removed=0 modified=0 invalid=0"* ||
      "$reload_status" != *"source_capability=reuse"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: rejection changed accepted baseline: $reload_status"
	error_exit 1
fi

sed '/    name: first/i\    flowControl: "off"' "$CONF_FILE.startup" >"$CONF_FILE"
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
