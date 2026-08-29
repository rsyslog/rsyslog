#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Verify reserved egress never treats either the source dequeue ceiling or a
# publication batch as a minimum fill requirement. A quiet-queue singleton
# must reach the target before any later arrival. After a burst drains, its
# singleton tail must likewise arrive before shutdown supplies another event.
# workerThreadMinimumMessages=1 makes worker activation explicit; no queue uses
# minDequeueBatchSize. Each wait_file_lines is the pre-next-arrival oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="reservedBatch")
main_queue(queue.type="LinkedList" queue.dequeueBatchSize="1024"
           queue.workerThreads="1" queue.workerThreadMinimumMessages="1")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="256"
        queue.dequeueBatchSize="1024" queue.workerThreads="1"
        queue.workerThreadMinimumMessages="1") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then call target
'
startup

# The first singleton is observed while the input queue is otherwise quiet.
injectmsg 0 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1

# Drain a burst, then verify a final singleton/tail without a later wakeup.
injectmsg 1 32
wait_file_lines "$RSYSLOG_OUT_LOG" 33
injectmsg 33 1
wait_file_lines "$RSYSLOG_OUT_LOG" 34

shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 34 "$RSYSLOG_OUT_LOG"
exit_test
