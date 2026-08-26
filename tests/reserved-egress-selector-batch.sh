#!/bin/bash
# Atomically flip a test-only selector override after the first element of a
# guaranteed two-element source batch. This does not emulate configuration
# reload; it probes that both legacy->reserved and reserved->legacy cases keep
# the selector captured at batch entry. Ordered 0,1 target output is the oracle;
# rereading the override can reverse publication order or use an uninitialized
# ledger.
if [ "$1" != "--case" ]; then
  for engine in legacy reservedBatch; do
    RSYSLOG_TEST_EGRESS_FLIP_AFTER_FIRST=1 RSTB_CASE_ENGINE=$engine "$0" --case || exit $?
  done
  exit 0
fi

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="'$RSTB_CASE_ENGINE'")
main_queue(queue.type="LinkedList" queue.dequeueBatchSize="2"
           queue.minDequeueBatchSize="2" queue.minDequeueBatchSize.timeout="5000")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="10") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then call target
'
startup
injectmsg 0 2
shutdown_when_empty
wait_shutdown
seq_check 0 1
exit_test
