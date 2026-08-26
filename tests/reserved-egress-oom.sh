#!/bin/bash
# Inject one allocation failure at each WTI ledger growth/prepare boundary.
# Each failure must leave the source uncommitted, release its duplicate exactly
# once, and leave the source retryable. A second, nonmatching source message
# wakes the main queue after the injected failure; exact one-line target
# delivery in every isolated child is the retry/ownership oracle.
if [ "$1" != "--case" ]; then
  for fault in bucket entry prepare; do
    RSYSLOG_TEST_EGRESS_ALLOC_FAIL=$fault "$0" --case || exit $?
  done
  exit 0
fi

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(executionEngine="reservedBatch")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="2") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum:00000000" then call target
'
startup
injectmsg 0 1
injectmsg 1 1
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 1 "$RSYSLOG_OUT_LOG"
exit_test
