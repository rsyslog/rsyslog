#!/bin/bash
# A fixed numeric plain-ptcp endpoint can move to a distinct socket while
# established sessions on the old socket continue delivering. The M2 reserver
# keeps the candidate port bound to force prepare rollback; after releasing it,
# successful replacement plus pending->0 after EOF and a successful rebind of
# the old port prove rollback, session drain, and resource reclamation. A TLS
# mode-1 change on the same canonical socket must remain restart-required.
# Validate mode reports both classifications without activation. Status fields,
# connect success/refusal, and routed records are the oracles; bounded polling
# sleeps only throttle status queries while the automatic retry runs.
# SPDX-License-Identifier: Apache-2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
MODE="${RSYSLOG_RELOAD_ENDPOINT_MODE:-on}"
case "$MODE" in on|validate) ;; *) error_exit 1 ;; esac
CONFIG_FORMAT="${RSYSLOG_RELOAD_CONFIG_FORMAT:-rainerscript}"
case "$CONFIG_FORMAT" in rainerscript|yaml) ;; *) error_exit 1 ;; esac
command -v python3 >/dev/null 2>&1 || error_exit 1
if [[ "$CONFIG_FORMAT" == yaml ]]; then
	require_yaml_support
fi

start_port_reserver() {
	coproc TCP_PORT_RESERVER { python3 "$srcdir/helpers/reload_tcp_port_reserver.py"; }
	PORT_RESERVER_PID=$TCP_PORT_RESERVER_PID
	if ! exec {PORT_RESERVER_READ_FD}<&"${TCP_PORT_RESERVER[0]}"; then
		error_exit 1
	fi
	if ! exec {PORT_RESERVER_WRITE_FD}>&"${TCP_PORT_RESERVER[1]}"; then
		exec {PORT_RESERVER_READ_FD}<&-
		error_exit 1
	fi
	PORT_RESERVER_ACTIVE=1
	if ! IFS=' ' read -r -t "$TB_TEST_TIMEOUT" reservation_state OLD_PORT NEW_PORT \
		<&"$PORT_RESERVER_READ_FD"; then
		echo "FAIL: timed out waiting for the port reservation helper"
		error_exit 1
	fi
	if [[ "$reservation_state" != READY || ! "$OLD_PORT" =~ ^[0-9]+$ ||
	      ! "$NEW_PORT" =~ ^[0-9]+$ || "$OLD_PORT" == "$NEW_PORT" ]]; then
		echo "FAIL: invalid port reservation helper response: $reservation_state $OLD_PORT $NEW_PORT"
		error_exit 1
	fi
}

release_reserved_port() {
	local number="$1"
	local response
	printf 'release %s\n' "$number" >&"$PORT_RESERVER_WRITE_FD" || error_exit 1
	if ! IFS= read -r -t "$TB_TEST_TIMEOUT" response <&"$PORT_RESERVER_READ_FD" ||
	   [[ "$response" != "RELEASED $number" ]]; then
		echo "FAIL: port reservation helper did not release port $number: ${response:-no response}"
		error_exit 1
	fi
}

stop_port_reserver() {
	local response
	if [[ "${PORT_RESERVER_ACTIVE:-0}" != 1 ]]; then
		return
	fi
	printf 'release all\n' >&"$PORT_RESERVER_WRITE_FD" || error_exit 1
	if ! IFS= read -r -t "$TB_TEST_TIMEOUT" response <&"$PORT_RESERVER_READ_FD" ||
	   [[ "$response" != 'RELEASED all' ]]; then
		echo "FAIL: port reservation helper did not release all sockets: ${response:-no response}"
		error_exit 1
	fi
	wait "$PORT_RESERVER_PID" || error_exit 1
	PORT_RESERVER_ACTIVE=0
	exec {PORT_RESERVER_READ_FD}<&-
	exec {PORT_RESERVER_WRITE_FD}>&-
}

test_error_exit_handler() {
	if [[ "${PORT_RESERVER_ACTIVE:-0}" == 1 ]]; then
		printf 'release all\n' 1>&"$PORT_RESERVER_WRITE_FD" 2>/dev/null || :
		IFS= read -r -t "$TB_TEST_TIMEOUT" _port_reserver_cleanup <&"$PORT_RESERVER_READ_FD" || :
		wait "$PORT_RESERVER_PID" 2>/dev/null || :
		PORT_RESERVER_ACTIVE=0
		exec {PORT_RESERVER_READ_FD}<&-
		exec {PORT_RESERVER_WRITE_FD}>&-
	fi
}

start_port_reserver
if [[ "$CONFIG_FORMAT" == yaml ]]; then
	generate_conf --yaml-only
	sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "'$MODE'"' "$TESTCONF_NM.yaml"
	add_yaml_conf '
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "'"$OLD_PORT"'"
    name: first
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
input(type="imtcp" address="127.0.0.1" port="'"$OLD_PORT"'" name="first" ruleset="main")
ruleset(name="main") {
  action(type="omfile" name="sink" file="'"$RSYSLOG_OUT_LOG"'")
}
'
fi
release_reserved_port 1
startup
[[ -n "$CONF_FILE" && -f "$CONF_FILE" ]] || error_exit 1
cp "$CONF_FILE" "$CONF_FILE.startup" || error_exit 1
ACTIVE_PORT="$OLD_PORT"
exec 9<>"/dev/tcp/127.0.0.1/$ACTIVE_PORT" || error_exit 1
printf '<167>Mar 10 01:00:00 host app: old-session-before-replacement\n' >&9 || error_exit 1
wait_content 'old-session-before-replacement' "$RSYSLOG_OUT_LOG"

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

set_candidate_port() {
	local port="$1"
	if [[ "$CONFIG_FORMAT" == yaml ]]; then
		sed 's/    port: "'$OLD_PORT'"/    port: "'$port'"/' \
			"$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
	else
		sed 's/port="'$OLD_PORT'"/port="'$port'"/' \
			"$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
	fi
}

set_same_socket_tls_candidate() {
	if [[ "$CONFIG_FORMAT" == yaml ]]; then
		awk '
			/^    name: first$/ {
				print "    StreamDriver.Name: \"gtls\""
				print "    StreamDriver.Mode: \"1\""
				print "    StreamDriver.AuthMode: \"anon\""
			}
			{ print }
		' "$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
	else
		sed 's/name="first"/StreamDriver.Name="gtls" StreamDriver.Mode="1" StreamDriver.AuthMode="anon" name="first"/' \
			"$CONF_FILE.startup" >"$CONF_FILE" || error_exit 1
	fi
}

assert_old_session_delivers() {
	local tag="$1"
	printf '<167>Mar 10 01:00:00 host app: old-session-%s\n' "$tag" >&9 || error_exit 1
	wait_content "old-session-$tag" "$RSYSLOG_OUT_LOG"
}

assert_listener_accepts() {
	local port="$1"
	local tag="$2"
	exec 7<>"/dev/tcp/127.0.0.1/$port" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: listener-accept-%s\n' "$tag" >&7 || error_exit 1
	exec 7>&-
	wait_content "listener-accept-$tag" "$RSYSLOG_OUT_LOG"
}

assert_listener_stopped() {
	local port="$1"
	if exec 6<>"/dev/tcp/127.0.0.1/$port" 2>/dev/null; then
		exec 6>&-
		echo "FAIL: retired listener on port $port accepted a new connection"
		error_exit 1
	fi
}

set_candidate_port "$NEW_PORT"
issue_HUP
reload_status="$(get_reload_status)"
expected_result=activation_failed
[[ "$MODE" == validate ]] && expected_result=reported_only
if [[ "$reload_status" != *"result=$expected_result active_generation=1"* ||
	  "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
	  "$reload_status" != *"source_capability=drain_replace"* ||
	  "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: bound replacement endpoint did not roll back atomically: $reload_status"
	error_exit 1
fi
assert_old_session_delivers after-bind-conflict
assert_listener_accepts "$OLD_PORT" after-bind-conflict

release_reserved_port 2
issue_HUP
reload_status="$(get_reload_status)"
if [[ "$MODE" == on ]]; then
	if [[ "$reload_status" != *"result=activated active_generation=2"* ||
	      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
	      "$reload_status" != *"source_capability=drain_replace"* ||
	      "$reload_status" != *"retirement_pending=1"* ]]; then
		echo "FAIL: distinct numeric endpoint replacement did not retain old session: $reload_status"
		error_exit 1
	fi
	assert_old_session_delivers during-replacement-drain
	assert_listener_stopped "$OLD_PORT"
	exec 7<>"/dev/tcp/127.0.0.1/$NEW_PORT" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: replacement-session-before-next-reload\n' >&7 || error_exit 1
	wait_content 'replacement-session-before-next-reload' "$RSYSLOG_OUT_LOG"
	exec 9>&-
	wait_retirement_pending 0
	ACTIVE_PORT="$NEW_PORT"
	set_candidate_port "$OLD_PORT"
	issue_HUP
	reload_status="$(get_reload_status)"
	if [[ "$reload_status" != *"result=activated active_generation=3"* ||
	      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
	      "$reload_status" != *"source_capability=drain_replace"* ||
	      "$reload_status" != *"retirement_pending=1"* ]]; then
		echo "FAIL: reclaimed original endpoint could not be rebound while retiring the replacement session: $reload_status"
		error_exit 1
	fi
	printf '<167>Mar 10 01:00:00 host app: replacement-session-after-next-reload\n' >&7 || error_exit 1
	wait_content 'replacement-session-after-next-reload' "$RSYSLOG_OUT_LOG"
	assert_listener_stopped "$NEW_PORT"
	exec 7>&-
	wait_retirement_pending 0
	ACTIVE_PORT="$OLD_PORT"
	assert_listener_accepts "$ACTIVE_PORT" reclaimed-old-port
	# Keep a session through the incompatible candidate to verify neither the
	# socket nor its current plaintext profile changes on rejection.
	exec 8<>"/dev/tcp/127.0.0.1/$ACTIVE_PORT" || error_exit 1
else
	if [[ "$reload_status" != *"result=reported_only active_generation=1"* ||
	      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
	      "$reload_status" != *"source_capability=drain_replace"* ||
	      "$reload_status" != *"retirement_pending=0"* ]]; then
		echo "FAIL: validate mode did not report distinct-endpoint replacement: $reload_status"
		error_exit 1
	fi
	assert_old_session_delivers validate-no-activation
	assert_listener_accepts "$OLD_PORT" validate-no-activation
	ACTIVE_PORT="$OLD_PORT"
	exec 8<>"/dev/tcp/127.0.0.1/$ACTIVE_PORT" || error_exit 1
fi

if [[ -f ../runtime/.libs/lmnsd_gtls.so ]]; then
	set_same_socket_tls_candidate
	issue_HUP
	reload_status="$(get_reload_status)"
	expected_result=candidate_scope_unsupported
	expected_generation=3
	[[ "$MODE" == validate ]] && {
		expected_result=reported_only
		expected_generation=1
	}
	if [[ "$reload_status" != *"result=$expected_result active_generation=$expected_generation"* ||
	      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
	      "$reload_status" != *"source_capability=restart_required"* ||
	      "$reload_status" != *"retirement_pending=0"* ]]; then
		echo "FAIL: same-socket plain-to-TLS change was not restart-required: $reload_status"
		error_exit 1
	fi
	printf '<167>Mar 10 01:00:00 host app: same-socket-session-after-rejection\n' >&8 || error_exit 1
	wait_content 'same-socket-session-after-rejection' "$RSYSLOG_OUT_LOG"
	assert_listener_accepts "$ACTIVE_PORT" after-same-socket-rejection
else
	echo "info: skipping same-socket TLS rejection subcase; lmnsd_gtls was not built"
fi

exec 8>&-
stop_port_reserver
shutdown_when_empty
wait_shutdown
exec 9>&-
exit_test
