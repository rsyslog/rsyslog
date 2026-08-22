#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# %jsonmesg% on a turbo-only message must carry the normalized fields under
# "$!". Every output record is checked: the line count must match, every line
# must contain a "$!" object with the num field, and the extracted num values
# must form the complete sequence.
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

template(name="jm" type="string" string="%jsonmesg%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="jm")
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown

lines=$(wc -l < "$RSYSLOG_OUT_LOG")
if [ "$lines" -ne "$NUMMESSAGES" ]; then
	printf 'expected %s jsonmesg lines, got %s\n' "$NUMMESSAGES" "$lines"
	error_exit 1
fi
# every record: "$!" is an object holding num
with_num=$(grep -c -E '"\$!": *\{[^}]*"num": *"?[0-9]+' "$RSYSLOG_OUT_LOG")
if [ "$with_num" -ne "$NUMMESSAGES" ]; then
	printf 'only %s of %s jsonmesg records carry $!num\n' "$with_num" "$NUMMESSAGES"
	head -n 3 "$RSYSLOG_OUT_LOG"
	error_exit 1
fi
sed -E 's/.*"\$!": *\{[^}]*"num": *"?([0-9]+).*/\1/' "$RSYSLOG_OUT_LOG" \
	> "$RSYSLOG_DYNNAME.num.log"
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.num.log"
seq_check 0 $((NUMMESSAGES - 1))
exit_test
