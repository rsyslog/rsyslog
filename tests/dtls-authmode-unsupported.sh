#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Validates handling of an unsupported tls.authmode value in omdtls (action)
# and imdtls (input() and module()). Older versions silently fell back to
# anonymous mode, so a typo such as "fingerprnt" disabled peer verification.
# Policy under test, following global(compatibility.defaults.secure=...):
# - strict: the configuration is rejected (config validation fails and the
#   "unsupported tls.authmode" error is logged)
# - warn (default) and backward-compatible: validation succeeds and a warning
#   says that anonymous mode without peer verification is used
# - the explicit values "anon" and "certanon" stay valid without a warning.
# Oracle: rsyslogd -N1 exit status and its diagnostics; no runtime traffic.
# DTLS_AUTHMODE_YAML=YES writes the same configurations through the YAML
# frontend (yaml-dtls-authmode-unsupported.sh); the oracle is unchanged.
. ${srcdir:=.}/diag.sh init
require_plugin imdtls
require_plugin omdtls
if [ "${DTLS_AUTHMODE_YAML:-}" = YES ]; then
	require_yaml_support
fi

modpath="${RSYSLOG_MODDIR}"

run_check() {
	local expect="$1" cfg="$2" log="$3"
	../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
	local rc=$?
	if { [ "$expect" = ok ] && [ $rc -ne 0 ]; } || { [ "$expect" = fail ] && [ $rc -eq 0 ]; }; then
		echo "FAIL: expected ${cfg} to ${expect} validation (rc=$rc)"
		cat "${log}"
		error_exit 1
	fi
}

# $1 = policy, $2 = where (omdtls|imdtls-input|imdtls-module), $3 = authmode
write_yaml_config() {
	local cfg="${RSYSLOG_DYNNAME}.$1.$2.$3.yaml"
	{
		echo 'version: 2'
		echo 'global:'
		echo "  compatibility.defaults.secure: \"$1\""
		echo 'modules:'
		case "$2" in
		omdtls)
			echo '  - load: "../plugins/omdtls/.libs/omdtls"'
			;;
		imdtls-input)
			echo '  - load: "../plugins/imdtls/.libs/imdtls"'
			echo 'inputs:'
			echo '  - type: imdtls'
			echo '    port: "0"'
			echo "    tls.authmode: \"$3\""
			echo '    ruleset: main'
			;;
		imdtls-module)
			echo '  - load: "../plugins/imdtls/.libs/imdtls"'
			echo "    tls.authmode: \"$3\""
			echo 'inputs:'
			echo '  - type: imdtls'
			echo '    port: "0"'
			echo '    ruleset: main'
			;;
		esac
		echo 'rulesets:'
		echo '  - name: main'
		echo '    actions:'
		if [ "$2" = omdtls ]; then
			echo '      - type: omdtls'
			echo '        target: "127.0.0.1"'
			echo '        port: "4433"'
			echo "        tls.authmode: \"$3\""
		fi
		echo '      - type: omfile'
		echo '        file: "/dev/null"'
	} >"${cfg}"
	printf '%s' "${cfg}"
}

write_config() {
	if [ "${DTLS_AUTHMODE_YAML:-}" = YES ]; then
		write_yaml_config "$@"
		return
	fi
	local cfg="${RSYSLOG_DYNNAME}.$1.$2.$3.conf"
	{
		echo "global(compatibility.defaults.secure=\"$1\")"
		case "$2" in
		omdtls)
			echo 'module(load="../plugins/omdtls/.libs/omdtls")'
			echo "action(type=\"omdtls\" target=\"127.0.0.1\" port=\"4433\" tls.authmode=\"$3\")"
			;;
		imdtls-input)
			echo 'module(load="../plugins/imdtls/.libs/imdtls")'
			echo "input(type=\"imdtls\" port=\"0\" tls.authmode=\"$3\")"
			;;
		imdtls-module)
			echo "module(load=\"../plugins/imdtls/.libs/imdtls\" tls.authmode=\"$3\")"
			echo 'input(type="imdtls" port="0")'
			;;
		esac
		echo 'action(type="omfile" file="/dev/null")'
	} >"${cfg}"
	printf '%s' "${cfg}"
}

unsupported='unsupported tls.authmode "fingerprnt"'
fallback_warning='using anonymous mode WITHOUT peer verification'
strict_error='compatibility.defaults.secure="strict" rejects this configuration'

for where in omdtls imdtls-input imdtls-module; do
	cfg=$(write_config strict "$where" fingerprnt)
	run_check fail "$cfg" "$cfg.log"
	content_check "$unsupported" "$cfg.log"
	content_check "$strict_error" "$cfg.log"

	for policy in warn backward-compatible; do
		cfg=$(write_config "$policy" "$where" fingerprnt)
		run_check ok "$cfg" "$cfg.log"
		content_check "$unsupported" "$cfg.log"
		content_check "$fallback_warning" "$cfg.log"
	done

	for mode in anon certanon; do
		cfg=$(write_config strict "$where" "$mode")
		run_check ok "$cfg" "$cfg.log"
		check_not_present "unsupported tls.authmode" "$cfg.log"
	done
done

exit_test
