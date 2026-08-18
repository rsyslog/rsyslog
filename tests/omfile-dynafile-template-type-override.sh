#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Ensure legacy dynafile template types remain compatible under the fallback path guard.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1

generate_conf
add_conf '
template(name="dynfile" type="subtree" subtree="$!dynfile")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	set $!dynfile = "'$RSYSLOG_OUT_LOG'";
	action(type="omfile" dynafile="dynfile" template="outfmt")
}
'

if ! ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f"${TESTCONF_NM}.conf" \
	>"${RSYSLOG_DYNNAME}.log" 2>&1; then
	echo "FAIL: dynafile template type override warning rejected the configuration"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

startup
injectmsg 0 "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
