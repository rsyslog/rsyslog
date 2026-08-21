#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# unset $! must drop the turbo snapshot together with the JSON tree, both
# when the message is still turbo-only and after the tree has been
# materialized. After each unset, both the single-field getter (%$!num%) and
# the full tree (%$!%) must be empty for every record. A third output written
# before the first unset proves the pipeline did normalize every record.
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
template(name="both" type="string" string="%$!num%|%$!%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.seq.log" template="num")
	# turbo-only unset: no JSON tree exists yet
	unset $!;
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="both")
	# fresh snapshot, materialize it, then unset
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	set $.tree = $!;
	unset $!;
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="both")
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.seq.log"
seq_check 0 $((NUMMESSAGES - 1))
export SEQ_CHECK_FILE=""

check_all_empty() {
	local file="$1" label="$2" lines empty
	lines=$(wc -l < "$file")
	empty=$(grep -c -x '|' "$file")
	if [ "$lines" -ne "$NUMMESSAGES" ] || [ "$empty" -ne "$NUMMESSAGES" ]; then
		printf '%s: %s lines, %s fully empty, expected %s of each\n' \
			"$label" "$lines" "$empty" "$NUMMESSAGES"
		grep -v -x '|' "$file" | head -n 3
		error_exit 1
	fi
}
check_all_empty "$RSYSLOG_OUT_LOG" "turbo-only unset"
check_all_empty "$RSYSLOG2_OUT_LOG" "unset after materialize"
exit_test
