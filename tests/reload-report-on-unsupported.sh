#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: config.reloadOnHUP="on" is reserved for the future live reload and
# must be refused for now with a clear error at config load; an unknown value
# is refused as well, while "off" and "report" load cleanly. Checked with
# "rsyslogd -N1" for RainerScript and YAML.
#
# Oracle: exit status (non-zero only for "on" and the bogus value) and the
# message text "config.reloadOnHUP=\"on\" is not yet supported".
. ${srcdir:=.}/diag.sh init
require_yaml_support

check_mode() { # $1 mode, $2 rs|yaml, $3 expected: ok|fail
	local f="${RSYSLOG_DYNNAME}.$1.$2"
	if [ "$2" = yaml ]; then
		printf 'global:\n  config.reloadOnHUP: "%s"\nrulesets:\n  - name: main\n    script: |\n      action(type="omfile" file="/dev/null")\n' "$1" > "$f"
	else
		printf 'global(config.reloadOnHUP="%s")\n*.* /dev/null\n' "$1" > "$f"
	fi
	../tools/rsyslogd -N1 -f "$f" -M"$RSYSLOG_MODDIR" > "$f.out" 2>&1
	rc=$?
	if [ "$3" = ok ] && [ $rc -ne 0 ]; then
		echo "FAIL: mode $1 ($2) must load, rc=$rc"; cat -n "$f.out"; error_exit 1
	fi
	if [ "$3" = fail ] && [ $rc -eq 0 ]; then
		echo "FAIL: mode $1 ($2) must be refused, rc=0"; cat -n "$f.out"; error_exit 1
	fi
	if [ "$1" = on ] && ! grep -qF 'config.reloadOnHUP="on" is not yet supported' "$f.out"; then
		echo "FAIL: mode on ($2): expected message not found"; cat -n "$f.out"; error_exit 1
	fi
}
for fe in rs yaml; do
	check_mode off $fe ok
	check_mode report $fe ok
	check_mode on $fe fail
	check_mode bogus $fe fail
done
exit_test
