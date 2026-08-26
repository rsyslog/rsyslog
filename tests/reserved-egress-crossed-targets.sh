#!/bin/bash
# Force two source WTIs to reserve different one-slot targets, synchronize at a
# direct-action barrier, and then request the other WTI's target. Both targets
# receiving both messages is the deterministic oracle: flushing only the
# pressured bucket times out and loses the crossed branches, whereas flushing
# all locally held buckets starts both target workers and permits progress.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="reservedBatch")
main_queue(queue.type="LinkedList" queue.workerThreads="2"
           queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1")
module(load="../plugins/omtesting/.libs/omtesting")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="barrier") {
  :omtesting:barrier_error 2
}
ruleset(name="target_a" queue.type="LinkedList" queue.size="1" queue.timeoutEnqueue="5000") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.a" template="outfmt")
}
ruleset(name="target_b" queue.type="FixedArray" queue.size="1" queue.timeoutEnqueue="5000") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.b" template="outfmt")
}
if $msg contains "00000000" then {
  call target_a
  call barrier
  call target_b
} else if $msg contains "00000001" then {
  call target_b
  call barrier
  call target_a
}
'
startup
injectmsg 0 2
shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 2 "$RSYSLOG_OUT_LOG.a"
content_count_check "msgnum" 2 "$RSYSLOG_OUT_LOG.b"
exit_test
