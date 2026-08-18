#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Ensure strict dynafile template-type mode rejects uninspectable templates for
# legacy selector actions as well as modern actions.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="builtin:omfile" dynafile.restrictTemplateType="on")

template(name="dynfile" type="subtree" subtree="$!dynfile")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

*.* ?dynfile;outfmt
'

if ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
	>"${RSYSLOG_DYNNAME}.log" 2>&1; then
	echo "FAIL: expected config validation failure for subtree dynafile template"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
else
	exit_code=$?
	if [ "$exit_code" -ne 1 ]; then
		echo "FAIL: expected exit code 1, got $exit_code"
		cat "${RSYSLOG_DYNNAME}.log"
		error_exit 1
	fi
fi

grep -F "dynafile template 'dynfile' uses a template type that cannot be safely inspected" \
	"${RSYSLOG_DYNNAME}.log" >/dev/null || {
	echo "FAIL: expected dynafile template type validation error"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
}

exit_test
