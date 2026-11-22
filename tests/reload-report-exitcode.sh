#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: the verdict of config.reloadOnHUP="report" is the exit status of
# "rsyslogd -N1". This test asserts what the feature relies on: -N1 exits 0
# for a valid config and non-zero for an invalid one or a missing file, for
# RainerScript and YAML. Valid configs include a warning-only case (plain
# imtcp emits an insecure-default warning), which must still exit 0.
#
# Oracle: the exit codes of the foreground rsyslogd -N1 runs below.
. ${srcdir:=.}/diag.sh init
# registered only with libyaml and imtcp (see tests/Makefile.am)
good_rs="${RSYSLOG_DYNNAME}.good.conf"
warn_rs="${RSYSLOG_DYNNAME}.warn.conf"
bad_rs="${RSYSLOG_DYNNAME}.bad.conf"
good_yaml="${RSYSLOG_DYNNAME}.good.yaml"
bad_yaml="${RSYSLOG_DYNNAME}.bad.yaml"
printf '*.* /dev/null\n' > "$good_rs"
printf 'module(load="../plugins/imtcp/.libs/imtcp")\ninput(type="imtcp" port="0")\n*.* /dev/null\n' > "$warn_rs"
printf 'if $msg contains then {\n' > "$bad_rs"
printf 'templates:\n  - name: t\n    type: string\n    string: "x"\nrulesets:\n  - name: main\n    script: |\n      action(type="omfile" file="/dev/null")\n' > "$good_yaml"
printf 'templates: [ not : valid\n' > "$bad_yaml"

check_rc() { # $1 expected: "zero" or "nonzero", $2 config file
	../tools/rsyslogd -N1 -f "$2" -M"$RSYSLOG_MODDIR" > "$2.out" 2>&1
	rc=$?
	if { [ "$1" = zero ] && [ $rc -ne 0 ]; } || { [ "$1" = nonzero ] && [ $rc -eq 0 ]; }; then
		echo "FAIL: rsyslogd -N1 -f $2 returned $rc, expected $1"
		cat -n "$2.out"
		error_exit 1
	fi
	echo "ok: -N1 -f $2 returned $rc ($1 expected)"
}
check_rc zero "$good_rs"
check_rc zero "$warn_rs"
check_rc zero "$good_yaml"
check_rc nonzero "$bad_rs"
check_rc nonzero "$bad_yaml"
check_rc nonzero "${RSYSLOG_DYNNAME}.does-not-exist.conf"
exit_test
