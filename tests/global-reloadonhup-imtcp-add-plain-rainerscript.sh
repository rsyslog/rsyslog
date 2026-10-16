#!/bin/bash
# A fixed numeric plain-ptcp listener may be added by HUP without losing an
# old session. The added listener must route to its pre-existing ruleset, be
# reused by an identical HUP, and accept a later live profile update. A held
# loopback reservation supplies numeric ports without get_free_port: release
# is acknowledged immediately before HUP; any close/rebind collision fails
# closed. In validate mode the reservation stays held while status must report
# the addition as supported without advancing the generation or binding it.
# The on-mode two-addition case also proves prepare rollback: the first socket
# is released, the second remains reserved to force bind failure, and retrying
# that exact first port must then succeed. Explicit helper handshakes use the
# testbench timeout only to fail closed if the owned process stops responding;
# no fixed sleep is an oracle. TLS and named-rate-profile additions are outside
# this support boundary: their held-port status plus usable old traffic prove
# classification rejects them before private listener activation. The TLS
# subcase is skipped only when the GnuTLS netstream driver was not built.
# SPDX-License-Identifier: Apache-2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
MODE="${RSYSLOG_RELOAD_ENDPOINT_MODE:-on}"
case "$MODE" in on|validate) ;; *) error_exit 1 ;; esac
CONFIG_FORMAT="${RSYSLOG_RELOAD_CONFIG_FORMAT:-rainerscript}"
case "$CONFIG_FORMAT" in rainerscript|yaml) ;; *) error_exit 1 ;; esac
command -v python3 >/dev/null 2>&1 || error_exit 1

start_port_reserver() {
	coproc TCP_PORT_RESERVER { python3 "$srcdir/helpers/reload_tcp_port_reserver.py"; }
	PORT_RESERVER_PID=$TCP_PORT_RESERVER_PID
	# Copy the coprocess descriptors before any wait can let Bash discard its
	# automatically managed array after child exit.
	if ! exec {PORT_RESERVER_READ_FD}<&"${TCP_PORT_RESERVER[0]}"; then
		error_exit 1
	fi
	if ! exec {PORT_RESERVER_WRITE_FD}>&"${TCP_PORT_RESERVER[1]}"; then
		exec {PORT_RESERVER_READ_FD}<&-
		error_exit 1
	fi
	PORT_RESERVER_ACTIVE=1
	if ! IFS=' ' read -r -t "$TB_TEST_TIMEOUT" reservation_state ADDED_PORT CONFLICT_PORT \
		<&"$PORT_RESERVER_READ_FD"; then
		echo "FAIL: timed out waiting for the port reservation helper"
		error_exit 1
	fi
	if [[ "$reservation_state" != READY || ! "$ADDED_PORT" =~ ^[0-9]+$ ||
	      ! "$CONFLICT_PORT" =~ ^[0-9]+$ || "$ADDED_PORT" == "$CONFLICT_PORT" ]]; then
		echo "FAIL: invalid port reservation helper response: $reservation_state $ADDED_PORT $CONFLICT_PORT"
		error_exit 1
	fi
}

release_reserved_port1() {
	local response
	printf 'release 1\n' >&"$PORT_RESERVER_WRITE_FD" || error_exit 1
	if ! IFS= read -r -t "$TB_TEST_TIMEOUT" response <&"$PORT_RESERVER_READ_FD" ||
	   [[ "$response" != 'RELEASED 1' ]]; then
		echo "FAIL: port reservation helper did not release port 1: ${response:-no response}"
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

add_candidate_inputs() {
	local include_conflict="$1"
	if [[ "$CONFIG_FORMAT" == yaml ]]; then
		awk -v added_port="$ADDED_PORT" -v conflict_port="$CONFLICT_PORT" \
		    -v include_conflict="$include_conflict" '
			/^rulesets:/ {
				print "  - type: imtcp"
				print "    address: \"127.0.0.1\""
				print "    port: \"" added_port "\""
				print "    name: added"
				print "    ruleset: added"
				if (include_conflict == "yes") {
					print "  - type: imtcp"
					print "    address: \"127.0.0.1\""
					print "    port: \"" conflict_port "\""
					print "    name: conflict"
					print "    ruleset: added"
				}
			}
			{ print }
		' "$CONF_FILE.startup" >"$CONF_FILE"
	else
		awk -v added_port="$ADDED_PORT" -v conflict_port="$CONFLICT_PORT" \
		    -v include_conflict="$include_conflict" '
			/^ruleset\(name="main"\)/ {
				print "input(type=\"imtcp\" address=\"127.0.0.1\" port=\"" added_port "\" name=\"added\" ruleset=\"added\")"
				if (include_conflict == "yes")
					print "input(type=\"imtcp\" address=\"127.0.0.1\" port=\"" conflict_port "\" name=\"conflict\" ruleset=\"added\")"
			}
			{ print }
		' "$CONF_FILE.startup" >"$CONF_FILE"
	fi
}

add_unsupported_candidate() {
	local kind="$1"
	if [[ "$CONFIG_FORMAT" == yaml ]]; then
		awk -v added_port="$ADDED_PORT" -v kind="$kind" '
			/^rulesets:/ {
				print "  - type: imtcp"
				print "    address: \"127.0.0.1\""
				print "    port: \"" added_port "\""
				print "    name: unsupported_" kind
				print "    ruleset: added"
				if (kind == "tls") {
					print "    StreamDriver.Name: \"gtls\""
					print "    StreamDriver.Mode: \"1\""
					print "    StreamDriver.AuthMode: \"anon\""
				} else {
					print "    ratelimit.name: policy_tight"
				}
			}
			{ print }
		' "$CONF_FILE.startup" >"$CONF_FILE"
	else
		local input_options
		if [[ "$kind" == tls ]]; then
			input_options='StreamDriver.Name="gtls" StreamDriver.Mode="1" StreamDriver.AuthMode="anon"'
		else
			input_options='ratelimit.name="policy_tight"'
		fi
		awk -v added_port="$ADDED_PORT" -v kind="$kind" -v input_options="$input_options" '
			/^ruleset\(name="main"\)/ {
				print "input(type=\"imtcp\" address=\"127.0.0.1\" port=\"" added_port "\" name=\"unsupported_" kind "\" ruleset=\"added\" " input_options ")"
			}
			{ print }
		' "$CONF_FILE.startup" >"$CONF_FILE"
	fi
}

assert_old_session_alive() {
	local tag="$1"
	printf '<167>Mar 10 01:00:00 host app: old-session-%s\n' "$tag" >&9 || error_exit 1
	wait_content "old-session-$tag" "$RSYSLOG_OUT_LOG"
}

assert_old_listener_accepts() {
	local tag="$1"
	exec 6<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1
	printf '<167>Mar 10 01:00:00 host app: old-accept-%s\n' "$tag" >&6 || error_exit 1
	exec 6>&-
	wait_content "old-accept-$tag" "$RSYSLOG_OUT_LOG"
}

assert_new_listener_routes() {
	local tag="$1"
	printf '<167>Mar 10 01:00:00 host app: new-route-%s\n' "$tag" >&7 || error_exit 1
	wait_content "new-route-$tag" "$RSYSLOG_DYNNAME.added.log"
	check_not_present "new-route-$tag" "$RSYSLOG_OUT_LOG"
}

if [[ "$CONFIG_FORMAT" == yaml ]]; then
	require_yaml_support
	generate_conf --yaml-only
	sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "'$MODE'"' "$TESTCONF_NM.yaml"
	add_yaml_conf '
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
ratelimits:
  - name: policy_tight
    interval: 60
    burst: 1
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'"$RSYSLOG_DYNNAME"'.tcpflood_port"
    name: first
    ruleset: main
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'"$RSYSLOG_DYNNAME"'.seed_port"
    name: seed
    ruleset: main
rulesets:
  - name: main
    actions:
      - type: omfile
        name: mainSink
        file: "'"$RSYSLOG_OUT_LOG"'"
  - name: added
    actions:
      - type: omfile
        name: addedSink
        file: "'"$RSYSLOG_DYNNAME"'.added.log"
'
else
	generate_conf
	add_conf '
global(config.reloadOnHUP="'$MODE'")
module(load="../plugins/imtcp/.libs/imtcp")
ratelimit(name="policy_tight" interval="60" burst="1")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port" name="first" ruleset="main")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.seed_port" name="seed" ruleset="main")
ruleset(name="main") {
  action(type="omfile" name="mainSink" file="'"$RSYSLOG_OUT_LOG"'")
}
ruleset(name="added") {
  action(type="omfile" name="addedSink" file="'"$RSYSLOG_DYNNAME"'.added.log")
}
'
fi
startup
wait_file_exists "$RSYSLOG_DYNNAME.seed_port"
SEED_PORT="$(<"$RSYSLOG_DYNNAME.seed_port")"
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1
exec 8<>"/dev/tcp/127.0.0.1/$SEED_PORT" || error_exit 1
cp "$CONF_FILE" "$CONF_FILE.startup"
start_port_reserver

assert_unsupported_candidate() {
	local kind="$1"
	local expected_result=candidate_scope_unsupported
	if [[ "$MODE" == validate ]]; then
		expected_result=reported_only
	fi
	add_unsupported_candidate "$kind"
	issue_HUP
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" != *"result=$expected_result active_generation=1"* ||
	      "$reload_status" != *"added=1 removed=0 modified=0 invalid=0"* ||
	      "$reload_status" != *"source_capability=restart_required"* ||
	      "$reload_status" != *"retirement_pending=0"* ]]; then
		echo "FAIL: unsupported $kind endpoint addition changed the accepted generation: $reload_status"
		error_exit 1
	fi
	assert_old_session_alive "after-unsupported-$kind"
	assert_old_listener_accepts "after-unsupported-$kind"
}

# Both reservations are still held. Each candidate starts from the unchanged
# startup config; the named policy exists there and only its new input reference
# changes. Old-session and fresh-accept traffic prove neither rejection
# disturbed the current listener generation.
assert_unsupported_candidate rate_limit
if [[ -f ../runtime/.libs/lmnsd_gtls.so ]]; then
	assert_unsupported_candidate tls
else
	echo "info: skipping TLS unsupported-addition subcase; lmnsd_gtls was not built"
fi

if [[ "$MODE" == validate ]]; then
	add_candidate_inputs no
	issue_HUP
	reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
	if [[ "$reload_status" != *"result=reported_only active_generation=1"* ||
	      "$reload_status" != *"added=1 removed=0 modified=0 invalid=0"* ||
	      "$reload_status" != *"source_capability=new_sessions"* ||
	      "$reload_status" != *"retirement_pending=0"* ]]; then
		echo "FAIL: validate mode did not report the fixed-port addition without activation: $reload_status"
		error_exit 1
	fi
	assert_old_session_alive validate
	assert_old_listener_accepts validate
	stop_port_reserver
	exec 8>&-
	exec 9>&-
	shutdown_when_empty
	wait_shutdown
	exit_test
fi

# First release only the candidate's first port. The second stays bound by
# this helper so the second prepared addition must fail at bind time.
add_candidate_inputs yes
release_reserved_port1
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activation_failed active_generation=1"* ||
      "$reload_status" != *"added=2 removed=0 modified=0 invalid=0"* ||
      "$reload_status" != *"source_capability=new_sessions"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: bind conflict did not abort the two-addition candidate atomically: $reload_status"
	error_exit 1
fi
assert_old_session_alive aborted-prepare
assert_old_listener_accepts aborted-prepare

# Retrying the identical first numeric port after removing the conflicting
# second input proves that failed preparation released its private listener.
add_candidate_inputs no
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=2"* ||
      "$reload_status" != *"added=1 removed=0 modified=0 invalid=0"* ||
      "$reload_status" != *"source_capability=new_sessions"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: fixed-port plain input addition did not activate: $reload_status"
	error_exit 1
fi
assert_old_session_alive after-addition
assert_old_listener_accepts after-addition
exec 7<>"/dev/tcp/127.0.0.1/$ADDED_PORT" || error_exit 1
assert_new_listener_routes first-accept

# Keep the second reservation until after a successful candidate so the test
# continues to prove that only explicitly released ports can be claimed.
stop_port_reserver

# An identical candidate reports reuse and leaves the accepted listener and
# its established session in place.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=2"* ||
      "$reload_status" != *"added=0 removed=0 modified=0 invalid=0"* ||
      "$reload_status" != *"source_capability=reuse"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: identical HUP did not reuse the accepted generation: $reload_status"
	error_exit 1
fi
assert_old_session_alive after-identical-hup
assert_new_listener_routes after-identical-hup

# Change only the fixed numeric endpoint's live profile.
if [[ "$CONFIG_FORMAT" == yaml ]]; then
	awk -v added_port="$ADDED_PORT" '
		{ print }
		$0 == "    port: \"" added_port "\"" { print "    flowControl: \"off\"" }
	' "$CONF_FILE" >"$CONF_FILE.profile"
else
	sed "s/port=\"$ADDED_PORT\" name=\"added\"/port=\"$ADDED_PORT\" flowControl=\"off\" name=\"added\"/" \
		"$CONF_FILE" >"$CONF_FILE.profile"
fi
mv "$CONF_FILE.profile" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=activated active_generation=3"* ||
      "$reload_status" != *"added=0 removed=0 modified=1 invalid=0"* ||
      "$reload_status" != *"source_capability=live_swap"* ||
      "$reload_status" != *"retirement_pending=0"* ]]; then
	echo "FAIL: live profile update on the added endpoint did not activate: $reload_status"
	error_exit 1
fi
assert_old_session_alive after-added-profile
assert_new_listener_routes after-added-profile
exec 8>&-
exec 9>&-
exec 7>&-
shutdown_when_empty
wait_shutdown
exit_test
