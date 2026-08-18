#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Ensure normal list dynafile templates are accepted by default.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="dynfile" type="list") {
	constant(value="./")
	property(name="hostname")
	constant(value="/")
	property(name="app-name")
	constant(value=".log")
}
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	action(type="omfile" dynafile="dynfile" template="outfmt")
}
'

if ! ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
	>"${RSYSLOG_DYNNAME}.log" 2>&1; then
	echo "FAIL: expected list dynafile template to be accepted by default"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

exit_test
