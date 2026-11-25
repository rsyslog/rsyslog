#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
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
. ${srcdir:=.}/diag.sh init
require_plugin imdtls
require_plugin omdtls

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
write_config() {
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
