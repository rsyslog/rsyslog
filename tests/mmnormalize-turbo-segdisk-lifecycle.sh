#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Turbo CEE fields must survive a segmented disk queue: the snapshot is a
# module-owned blob that cannot cross a queue file, so the segmented codec
# has to persist the materialized JSON tree like the classic disk queue
# does. Every record written after the queue must carry the field.
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
		rule="rule=:msgnum:%num:number%:")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num"
		queue.type="segmentedDisk" queue.filename="turbo-segdisk")
}
'
startup
tcpflood -m"$NUMMESSAGES"
# the queue must really be the segmented disk engine: its files carry the
# configured prefix in the work directory
segfiles=$(ls "$RSYSLOG_DYNNAME.spool" 2>/dev/null | grep -c '^turbo-segdisk')
if [ "$segfiles" -eq 0 ]; then
	printf 'no segmented disk queue files in %s.spool:\n' "$RSYSLOG_DYNNAME"
	ls -la "$RSYSLOG_DYNNAME.spool" 2>&1
	error_exit 1
fi
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1))
exit_test
