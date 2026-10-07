#!/bin/bash
# Removing a uniquely named port-0 imtcp listener stops new accepts while an
# established session remains usable; status reports retirement_pending until
# EOF and the automatic control retry releases the listener. A second removal
# keeps a session open through daemon shutdown to prove pending retirement does
# not outlive the tcpsrv runtime. HUP while retirement is pending fails before
# candidate parse and leaves the old session/listener state intact. Validate
# mode reports the same capability
# without changing generation or listener behavior. The status field and
# routed messages are the oracles; bounded status polling waits for the
# documented <=100 ms retirement retry. Its sleep only throttles status
# queries; the retirement_pending field proves completion, and TB_TEST_TIMEOUT
# is only the failure bound.
# SPDX-License-Identifier: Apache-2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
MODE="${RSYSLOG_RELOAD_ENDPOINT_MODE:-on}"
case "$MODE" in on|validate) ;; *) error_exit 1 ;; esac
CONFIG_FORMAT="${RSYSLOG_RELOAD_CONFIG_FORMAT:-rainerscript}"
case "$CONFIG_FORMAT" in rainerscript|yaml) ;; *) error_exit 1 ;; esac

if [[ "$CONFIG_FORMAT" == yaml ]]; then
	require_yaml_support
	generate_conf --yaml-only
	sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "'$MODE'"' "$TESTCONF_NM.yaml"
	add_yaml_conf '
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'"$RSYSLOG_DYNNAME"'.first_port"
    name: first
    ruleset: main
  # seed endpoint
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'"$RSYSLOG_DYNNAME"'.seed_port"
    name: seed
    ruleset: main
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'"$RSYSLOG_DYNNAME"'.anchor_port"
    name: anchor
    ruleset: main
rulesets:
  - name: main
    actions:
      - type: omfile
        name: sink
        file: "'"$RSYSLOG_OUT_LOG"'"
'
else
	generate_conf
	add_conf '
global(config.reloadOnHUP="'$MODE'")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.first_port" name="first" ruleset="main")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.seed_port" name="seed" ruleset="main")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.anchor_port" name="anchor" ruleset="main")
ruleset(name="main") {
  action(type="omfile" name="sink" file="'"$RSYSLOG_OUT_LOG"'")
}
'
fi

startup
wait_file_exists "$RSYSLOG_DYNNAME.first_port"
wait_file_exists "$RSYSLOG_DYNNAME.seed_port"
wait_file_exists "$RSYSLOG_DYNNAME.anchor_port"
FIRST_PORT="$(<"$RSYSLOG_DYNNAME.first_port")"
SEED_PORT="$(<"$RSYSLOG_DYNNAME.seed_port")"
exec 8<>"/dev/tcp/127.0.0.1/$FIRST_PORT" || error_exit 1
exec 9<>"/dev/tcp/127.0.0.1/$SEED_PORT" || error_exit 1
printf '<167>Mar 10 01:00:00 host app: first-session-before-removal\n' >&8 || error_exit 1
wait_content 'first-session-before-removal' "$RSYSLOG_OUT_LOG"
printf '<167>Mar 10 01:00:00 host app: seed-session-before-removal\n' >&9 || error_exit 1
wait_content 'seed-session-before-removal' "$RSYSLOG_OUT_LOG"
[[ -n "$CONF_FILE" && -f "$CONF_FILE" ]] || error_exit 1
cp "$CONF_FILE" "$CONF_FILE.startup" || error_exit 1

get_reload_status() {
	echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT"
}

wait_retirement_pending() {
	local expected="$1"
	local status
	local deadline=$((SECONDS + TB_TEST_TIMEOUT))
	while (( SECONDS <= deadline )); do
		status="$(get_reload_status)" || error_exit 1
		if [[ "$status" == *"retirement_pending=$expected"* ]]; then
			return 0
		fi
		sleep 0.1
	done
	echo "FAIL: retirement_pending=$expected was not observed: ${status:-no status}"
	error_exit 1
}

assert_live_first_listener() {
	local tag="$1"
	exec 7<>"/dev/tcp/127.0.0.1/$FIRST_PORT" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: first-accept-%s\n' "$tag" >&7 || error_exit 1
	exec 7>&-
	wait_content "first-accept-$tag" "$RSYSLOG_OUT_LOG"
}

assert_live_seed_listener() {
	local tag="$1"
	exec 7<>"/dev/tcp/127.0.0.1/$SEED_PORT" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: seed-accept-%s\n' "$tag" >&7 || error_exit 1
	exec 7>&-
	wait_content "seed-accept-$tag" "$RSYSLOG_OUT_LOG"
}

assert_removed_seed_stops_accepting() {
	if exec 6<>"/dev/tcp/127.0.0.1/$SEED_PORT" 2>/dev/null; then
		exec 6>&-
		echo "FAIL: removed listener accepted a new connection while its old session drained"
		error_exit 1
	fi
}

remove_seed_candidate() {
	if [[ "$CONFIG_FORMAT" == yaml ]]; then
		awk '
			function flush_block() {
				if (block != "" && !remove_block) printf "%s", block
				block = ""
				remove_block = 0
			}
			/^  - type: imtcp$/ { flush_block(); block = $0 "\n"; next }
			/^rulesets:$/ { flush_block(); print; next }
			block != "" {
				block = block $0 "\n"
				if ($0 == "    name: seed") remove_block = 1
				next
			}
			{ print }
			END { flush_block() }
		' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
	else
		sed '/name="seed"/d' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
	fi
}

remove_first_candidate() {
	if [[ "$CONFIG_FORMAT" == yaml ]]; then
		awk '
			function flush_block() {
				if (block != "" && !remove_block) printf "%s", block
				block = ""
				remove_block = 0
			}
			/^  - type: imtcp$/ { flush_block(); block = $0 "\n"; next }
			/^rulesets:$/ { flush_block(); print; next }
			block != "" {
				block = block $0 "\n"
				if ($0 == "    name: first") remove_block = 1
				next
			}
			{ print }
			END { flush_block() }
		' "$CONF_FILE.after-seed-removal" >"$CONF_FILE" || error_exit 1
	else
		sed '/name="first"/d' "$CONF_FILE.after-seed-removal" >"$CONF_FILE" || error_exit 1
	fi
}

remove_seed_candidate
issue_HUP
reload_status="$(get_reload_status)"
expected_result=activated
expected_generation=2
expected_pending=1
if [[ "$MODE" == validate ]]; then
	expected_result=reported_only
	expected_generation=1
	expected_pending=0
fi
if [[ "$reload_status" != *"result=$expected_result active_generation=$expected_generation"* ||
	  "$reload_status" != *"added=0 removed=1 modified=0 invalid=0"* ||
	  "$reload_status" != *"source_capability=drain_replace"* ||
	  "$reload_status" != *"retirement_pending=$expected_pending"* ]]; then
	echo "FAIL: named port-0 removal had unexpected status: $reload_status"
	error_exit 1
fi
printf '<167>Mar 10 01:00:00 host app: seed-session-after-remove\n' >&9 || error_exit 1
wait_content 'seed-session-after-remove' "$RSYSLOG_OUT_LOG"
if [[ "$MODE" == on ]]; then
	issue_HUP
	reload_status="$(get_reload_status)"
	if [[ "$reload_status" != *"result=activation_failed active_generation=2"* ||
	      "$reload_status" != *"added=0 removed=0 modified=0 invalid=0"* ||
	      "$reload_status" != *"source_capability=not_evaluated"* ||
	      "$reload_status" != *"retirement_pending=1"* ]]; then
		echo "FAIL: HUP during pending retirement changed or retried the candidate: $reload_status"
		error_exit 1
	fi
	printf '<167>Mar 10 01:00:00 host app: seed-session-after-pending-hup\n' >&9 || error_exit 1
	wait_content 'seed-session-after-pending-hup' "$RSYSLOG_OUT_LOG"
	assert_removed_seed_stops_accepting
	exec 9>&-
	wait_retirement_pending 0
	assert_live_first_listener after-seed-retired
	cp "$CONF_FILE" "$CONF_FILE.after-seed-removal" || error_exit 1
	remove_first_candidate
	issue_HUP
	reload_status="$(get_reload_status)"
	if [[ "$reload_status" != *"result=activated active_generation=3"* ||
		  "$reload_status" != *"added=0 removed=1 modified=0 invalid=0"* ||
		  "$reload_status" != *"source_capability=drain_replace"* ||
		  "$reload_status" != *"retirement_pending=1"* ]]; then
		echo "FAIL: second listener removal did not enter pending retirement: $reload_status"
		error_exit 1
	fi
	printf '<167>Mar 10 01:00:00 host app: first-session-before-shutdown\n' >&8 || error_exit 1
	wait_content 'first-session-before-shutdown' "$RSYSLOG_OUT_LOG"
	if exec 6<>"/dev/tcp/127.0.0.1/$FIRST_PORT" 2>/dev/null; then
		exec 6>&-
		echo "FAIL: second removed listener accepted a new connection while its old session drained"
		error_exit 1
	fi
	# Keep fd 8 open: clean shutdown must join and invalidate pending listener
	# retirement before the late shadow-reload cleanup runs.
	shutdown_when_empty
	wait_shutdown
	exec 8>&-
else
	assert_live_first_listener validate-kept-first
	assert_live_seed_listener validate-kept-seed
	exec 9>&-
	exec 8>&-
	shutdown_when_empty
	wait_shutdown
fi
exit_test
