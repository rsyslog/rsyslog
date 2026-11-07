#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# An explicit Docker DefaultFacility must preserve its syslog meaning. A
# loopback HTTP fixture supplies the Docker list/log endpoints and one binary
# stdout frame; no Docker daemon is needed. Unset-default, kern, and numeric
# ordinal controls precede explicit user/daemon/local0 assertions. Exact facility,
# severity, PRI and payload after owned daemon shutdown are the oracle.
# Invalid-value and value-type behavior is checked with -N1 only for
# compatibility; these controls never start a daemon or recommend invalid input.
# Waits bound real server readiness and record delivery, not elapsed startup.
. ${srcdir:=.}/diag.sh init
require_plugin imdocker ../contrib/imdocker
if [ "${IMDOCKER_FACILITY_YAML:-0}" = 1 ]; then
	require_yaml_support
fi
server_pid=
cleanup_server() {
	if [ -n "$server_pid" ]; then
		kill "$server_pid" 2>/dev/null || true
		wait "$server_pid" 2>/dev/null || true
	fi
}
trap cleanup_server EXIT
"$PYTHON" "$srcdir/imdocker-facility-server.py" "$RSYSLOG_DYNNAME.http.port" >"$RSYSLOG_DYNNAME.http.log" 2>&1 &
server_pid=$!
wait_file_exists_for_process "$RSYSLOG_DYNNAME.http.port" "$server_pid" "$TB_TIMEOUT_STARTSTOP" "Docker API fixture" "$RSYSLOG_DYNNAME.http.log"
api_port=$(cat "$RSYSLOG_DYNNAME.http.port")
for setting in unset kern 0 1 16 unknown empty negative numericjunk bare16 array user USER daemon local0; do
	value=$setting
	config_only=0
	expected_status=0
	# param is RainerScript configuration text, not shell quoting.
	# shellcheck disable=SC2089
	case "$setting" in
		unset) facility=1; param= ;;
		kern) facility=0; param='DefaultFacility="kern"' ;;
		0) facility=0; param='DefaultFacility="0"' ;;
		1) facility=1; param='DefaultFacility="1"' ;;
		16) facility=16; param='DefaultFacility="16"' ;;
		unknown) config_only=1; value=not-a-facility; param='DefaultFacility="not-a-facility"' ;;
		empty) config_only=1; value=; param='DefaultFacility=""' ;;
		negative) config_only=1; value=-1; param='DefaultFacility="-1"' ;;
		numericjunk) config_only=1; value=16junk; param='DefaultFacility="16junk"' ;;
		bare16) config_only=1; param='DefaultFacility=16';
			[ "${IMDOCKER_FACILITY_YAML:-0}" = 1 ] || expected_status=1 ;;
		array) config_only=1; expected_status=1; param='DefaultFacility=["user", "daemon"]' ;;
		user) facility=1; param='DefaultFacility="user"' ;;
		USER) facility=1; param='DefaultFacility="USER"' ;;
		daemon) facility=3; param='DefaultFacility="daemon"' ;;
		local0) facility=16; param='DefaultFacility="local0"' ;;
	esac
	rm -f "$RSYSLOG_OUT_LOG"
	generate_conf
	if [ "${IMDOCKER_FACILITY_YAML:-0}" = 1 ]; then
		cat >"$RSYSLOG_DYNNAME.yaml" <<EOF
version: 2
modules:
  - load: ../contrib/imdocker/.libs/imdocker
    DockerApiAddr: 'http://127.0.0.1:$api_port'
    ApiVersionStr: v1.27
    PollingInterval: 1
    GetContainerLogOptions: 'timestamps=0&follow=0&stdout=1&stderr=0&tail=all'
EOF
		case "$setting" in
			unset) ;;
			bare16) echo "    DefaultFacility: 16" >>"$RSYSLOG_DYNNAME.yaml" ;;
			array) echo '    DefaultFacility: ["user", "daemon"]' >>"$RSYSLOG_DYNNAME.yaml" ;;
			*) echo "    DefaultFacility: '$value'" >>"$RSYSLOG_DYNNAME.yaml" ;;
		esac
		cp "$RSYSLOG_DYNNAME.yaml" "$RSYSLOG_DYNNAME.$setting.yaml"
		add_conf 'include(file="'$RSYSLOG_DYNNAME.yaml'")'
	else
		# shellcheck disable=SC2090
		add_conf 'module(load="../contrib/imdocker/.libs/imdocker"
			DockerApiAddr="http://127.0.0.1:'$api_port'" ApiVersionStr="v1.27"
			PollingInterval="1" GetContainerLogOptions="timestamps=0&follow=0&stdout=1&stderr=0&tail=all" '"$param"')'
	fi
	add_conf '
template(name="facilityfmt" type="string" string="%syslogfacility%|%syslogseverity%|%pri%|%msg%\n")
if $inputname == "imdocker" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="facilityfmt")
'
	status=0
	../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"$RSYSLOG_DYNNAME.$setting.config.log" 2>&1 || status=$?
	if [ "$status" -ne "$expected_status" ]; then
		cat "$RSYSLOG_DYNNAME.$setting.config.log"
		error_exit 1
	fi
	cp "${TESTCONF_NM}.conf" "$RSYSLOG_DYNNAME.$setting.conf"
	if [ "$config_only" = 1 ]; then
		if [ "$expected_status" = 1 ]; then
			if [ "$setting" = array ]; then
				grep -q "parameter 'defaultfacility' is not a string" "$RSYSLOG_DYNNAME.$setting.config.log" || error_exit 1
			else
				grep -q "invalid character '1' in object definition" "$RSYSLOG_DYNNAME.$setting.config.log" || error_exit 1
			fi
		else
			grep -q "End of config validation run" "$RSYSLOG_DYNNAME.$setting.config.log" || error_exit 1
		fi
		echo "PASS: $setting facility config-only compatibility"
		continue
	fi
	startup
	wait_file_lines "$RSYSLOG_OUT_LOG" 1
	shutdown_when_empty
	wait_shutdown
	cp "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.$setting.output"
	printf '%s|6|%s|facility-marker\n' "$facility" "$((facility * 8 + 6))" >"$RSYSLOG_DYNNAME.expected"
	cmp "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG" || { echo "FAIL: facility setting $setting"; cat "$RSYSLOG_OUT_LOG"; error_exit 1; }
	echo "PASS: facility setting $setting"
done
cleanup_server
server_pid=
exit_test
