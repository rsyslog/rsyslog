#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: config.reloadOnHUP="on" is reserved for the future live reload and
# must be refused for now with a clear error at config load; an unknown value
# is refused as well, while "off" and "report" load cleanly. Checked with
# "rsyslogd -N1". RR_FRONTEND=yaml (reload-report-on-unsupported-yaml.sh) uses
# a YAML config instead of RainerScript.
#
# Oracle: exit status (non-zero only for "on" and the bogus value) and the
# message text "config.reloadOnHUP=\"on\" is not yet supported".
. ${srcdir:=.}/diag.sh init
FE=${RR_FRONTEND:-rs}
[ "$FE" = yaml ] && require_yaml_support

check_mode() { # $1 mode, $2 expected: ok|fail
	local f="${RSYSLOG_DYNNAME}.$1.$FE.conf"
	[ "$FE" = yaml ] && f="${RSYSLOG_DYNNAME}.$1.yaml"
	if [ "$FE" = yaml ]; then
		printf 'global:\n  config.reloadOnHUP: "%s"\nrulesets:\n  - name: main\n    script: |\n      action(type="omfile" file="/dev/null")\n' "$1" > "$f"
	else
		printf 'global(config.reloadOnHUP="%s")\n*.* /dev/null\n' "$1" > "$f"
	fi
	../tools/rsyslogd -N1 -f "$f" -M"$RSYSLOG_MODDIR" > "$f.out" 2>&1
	rc=$?
	if [ "$2" = ok ] && [ $rc -ne 0 ]; then
		echo "FAIL: mode $1 ($FE) must load, rc=$rc"; cat -n "$f.out"; error_exit 1
	fi
	if [ "$2" = fail ] && [ $rc -eq 0 ]; then
		echo "FAIL: mode $1 ($FE) must be refused, rc=0"; cat -n "$f.out"; error_exit 1
	fi
	if [ "$1" = on ] && ! grep -qF 'config.reloadOnHUP="on" is not yet supported' "$f.out"; then
		echo "FAIL: mode on ($FE): expected message not found"; cat -n "$f.out"; error_exit 1
	fi
}
check_mode off ok
check_mode report ok
check_mode on fail
check_mode bogus fail
exit_test
