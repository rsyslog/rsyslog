#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Exercise the pressure slow path with four source WTIs sharing a two-slot
# FixedArray target. Without partial publication, WTIs can hold both slots as
# invisible reservations and then self-block. Exact 500-line delivery after
# synchronized shutdown proves progress and ownership; the 5s enqueue timeout
# is hang protection, not the success oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="reservedBatch")
main_queue(queue.type="LinkedList" queue.workerThreads="4" queue.workerThreadMinimumMessages="1"
           queue.timeoutWorkerThreadShutdown="-1" queue.dequeueBatchSize="64")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="FixedArray" queue.size="2"
        queue.workerThreads="2" queue.timeoutEnqueue="5000") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then call target
'
startup
injectmsg 0 500
shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 500 "$RSYSLOG_OUT_LOG"
exit_test
