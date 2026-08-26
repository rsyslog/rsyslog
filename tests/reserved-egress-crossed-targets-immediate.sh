#!/bin/bash
# Repeat the crossed-target two-WTI schedule with timeoutEnqueue=0. Immediate
# admission may drop either crossed branch, so clean bounded shutdown plus one
# published prefix at each target is the delivery oracle. The direct barrier
# makes the crossed reservation schedule deterministic without timing sleeps.
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
ruleset(name="target_a" queue.type="LinkedList" queue.size="1" queue.timeoutEnqueue="0") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.a" template="outfmt")
}
ruleset(name="target_b" queue.type="FixedArray" queue.size="1" queue.timeoutEnqueue="0") {
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
content_check "msgnum" "$RSYSLOG_OUT_LOG.a"
content_check "msgnum" "$RSYSLOG_OUT_LOG.b"
exit_test
