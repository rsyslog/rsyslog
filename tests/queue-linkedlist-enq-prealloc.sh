#!/bin/bash
# LinkedList action queues are filled via qqueueEnqMsg(), which now allocates
# the list node before taking the queue mutex. A normal (non-direct) main
# queue feeds the action so inject uses MultiEnq on the main queue and
# qqueueEnqMsg() on the action queue. Oracle: every injected message is
# written in order after shutdown-when-empty.
#
# This file is part of rsyslog.
# Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES="${NUMMESSAGES:-5000}"
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
		queue.type="LinkedList" queue.size="10000"
		queue.dequeueBatchSize="128" queue.workerThreads="2")
}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
