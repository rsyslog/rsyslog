#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Turbo CEE fields must survive a segmented disk queue: the snapshot is a
# module-owned blob that cannot cross a queue file, so the segmented codec
# has to persist the materialized JSON tree like the classic disk queue
# does. Every record written after the queue must carry the field. A
# segmentedDisk action queue is disk-only, so every message crosses the codec;
# seq_check on the normalized field is the oracle.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

export NUMMESSAGES="${NUMMESSAGES:-1000}"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="normalize")

template(name="num" type="string" string="%$!num%\n")

ruleset(name="normalize") {
	action(type="mmnormalize" turbo="on"
		rule="rule=: msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num"
		queue.type="segmentedDisk" queue.filename="turbo-segdisk")
}
'
startup
tcpflood -m"$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1))
exit_test
