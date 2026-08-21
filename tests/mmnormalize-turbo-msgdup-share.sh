#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Exercise the stock topology that shares one turbo-normalized message
# across workers: an async action (default copyMsg=off, so MsgAddRef)
# materializes $! while a call into a queued ruleset MsgDup()s the same
# smsg_t. Both outputs must carry every sequence number: the MsgDup copy
# through %$!num%, the shared reference through the full %$!% tree, which
# is checked record by record. ASan/TSan builds of this test are the gate.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

export NUMMESSAGES="${NUMMESSAGES:-5000}"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

main_queue(
	queue.type="FixedArray"
	queue.size="20000"
	queue.workerThreads="8"
	queue.workerThreadMinimumMessages="1"
	queue.dequeueBatchSize="1"
)

input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="normalize")

template(name="num" type="string" string="%$!num%\n")
template(name="tree" type="string" string="%$!%\n")

ruleset(name="copy" queue.type="LinkedList" queue.workerThreads="8") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num"
		flushOnTXEnd="on")
}

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=:msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="tree"
		queue.type="LinkedList" queue.workerThreads="8"
		flushOnTXEnd="on")
	call copy
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1))
# The shared-reference path writes the full $! tree: one complete tree per
# record, and the num values must form the whole sequence.
lines=$(wc -l < "$RSYSLOG2_OUT_LOG")
trees=$(grep -c -E '^\{ *"num": *"?[0-9]+"? *\}$' "$RSYSLOG2_OUT_LOG")
if [ "$lines" -ne "$NUMMESSAGES" ] || [ "$trees" -ne "$NUMMESSAGES" ]; then
	printf 'full-tree output: %s lines, %s complete trees, expected %s\n' \
		"$lines" "$trees" "$NUMMESSAGES"
	grep -v -E '^\{ *"num": *"?[0-9]+"? *\}$' "$RSYSLOG2_OUT_LOG" | head -n 3
	error_exit 1
fi
sed -E 's/^\{ *"num": *"?([0-9]+).*/\1/' "$RSYSLOG2_OUT_LOG" > "$RSYSLOG_DYNNAME.num.log"
export SEQ_CHECK_FILE="$RSYSLOG_DYNNAME.num.log"
seq_check 0 $((NUMMESSAGES - 1))
exit_test
