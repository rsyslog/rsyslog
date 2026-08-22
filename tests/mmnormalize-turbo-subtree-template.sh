#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# type="subtree" templates on a turbo-only message, through a string-passing
# output (omfile) both synchronously and through an action queue. The full
# tree and a single field must be present for every record. Note that
# string-passing outputs render a subtree through tplToString(); the
# tplToJSON() path used by JSON-passing outputs is covered by the
# runtime_unit_turbo_msgdup unit test.
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

template(name="tree" type="subtree" subtree="$!")
template(name="field" type="subtree" subtree="$!num")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="tree"
		queue.type="LinkedList" queue.workerThreads="2")
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="field")
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

lines=$(wc -l < "$RSYSLOG_OUT_LOG")
trees=$(grep -c -E '^\{ *"num": *"?[0-9]+"? *\}$' "$RSYSLOG_OUT_LOG")
if [ "$lines" -ne "$NUMMESSAGES" ] || [ "$trees" -ne "$NUMMESSAGES" ]; then
	printf 'subtree $!: %s lines, %s complete trees, expected %s\n' \
		"$lines" "$trees" "$NUMMESSAGES"
	head -n 3 "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
sed -E 's/^\{ *"num": *"?([0-9]+).*/\1/' "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.num.log"
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.num.log"
seq_check 0 $((NUMMESSAGES - 1))
export SEQ_CHECK_FILE=""
seq_check2 0 $((NUMMESSAGES - 1))
exit_test
