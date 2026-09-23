#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# RainerScript exists() on a field that only lives in the turbo snapshot.
# Nothing touches $! before the test, so the message is still turbo-only when
# exists() runs. Every record must take the "exists" branch; the "missing"
# branch output must stay empty.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

export NUMMESSAGES="${NUMMESSAGES:-1000}"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="normalize")

template(name="num" type="string" string="%$!num%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	if exists($!num) then {
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num")
	} else {
		action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="num")
	}
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
if [ -s "$RSYSLOG2_OUT_LOG" ]; then
	printf 'exists($!num) was false for %s turbo-only records\n' \
		"$(wc -l < "$RSYSLOG2_OUT_LOG")"
	error_exit 1
fi
seq_check 0 $((NUMMESSAGES - 1))
exit_test
