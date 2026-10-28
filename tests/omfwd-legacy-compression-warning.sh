#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Legacy compression warns once per configured action, including implicit
# zipLevel selection. Disabled compression and stream/plain controls stay quiet.
# -N1 diagnostics are the oracle because no forwarding action runs in this test.
. "${srcdir:=.}/diag.sh" init

for scenario in explicit implicit octet udp disabled stream plain; do
	mode=single
	level=5
	protocol=tcp
	framing=
	case "$scenario" in
		implicit) mode= ;;
		octet) framing=octet-counted ;;
		udp) protocol=udp ;;
		disabled) level=0 ;;
		stream) mode=stream:always ;;
		plain) mode=; level= ;;
	esac
	generate_conf
	if [ "${LEGACY_WARNING_YAML:-0}" = 1 ]; then
		add_conf 'include(file="'${TESTCONF_NM}'.yaml")'
		: > "${TESTCONF_NM}.yaml"
		add_yaml_conf 'rulesets:'
		add_yaml_conf '  - name: compression-check'
		add_yaml_conf '    actions:'
		add_yaml_conf '      - type: omfwd'
		add_yaml_conf '        target: "127.0.0.1"'
		add_yaml_conf '        port: "1514"'
		add_yaml_conf "        protocol: \"$protocol\""
		[ -z "$mode" ] || add_yaml_conf "        compression.mode: \"$mode\""
		[ -z "$level" ] || add_yaml_conf "        zipLevel: \"$level\""
		[ -z "$framing" ] || add_yaml_conf "        TCP_Framing: \"$framing\""
	else
		parameters=
		[ -z "$mode" ] || parameters="$parameters compression.mode=\"$mode\""
		[ -z "$level" ] || parameters="$parameters zipLevel=\"$level\""
		[ -z "$framing" ] || parameters="$parameters TCP_Framing=\"$framing\""
		add_conf "action(type=\"omfwd\" target=\"127.0.0.1\" port=\"1514\" protocol=\"$protocol\" $parameters)"
	fi
	logfile="$RSYSLOG_DYNNAME.$scenario.validation"
	if ! ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"$logfile" 2>&1; then
		cat "$logfile"
		error_exit 1
	fi
	case "$scenario" in
		disabled|stream|plain)
			check_not_present "omfwd: legacy single-message z compression" "$logfile" ;;
		*)
			content_count_check "omfwd: legacy single-message z compression" 1 "$logfile" ;;
	esac
	check_not_present "received without octet-counted TCP framing" "$logfile"
done
exit_test
