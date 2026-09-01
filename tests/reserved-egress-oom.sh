#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Inject one allocation failure at each WTI ledger growth/prepare boundary.
# Each first-branch failure must leave the source uncommitted, release its
# duplicate exactly once, and leave the source retryable. The late case fails
# the second prepare after an earlier branch was accepted: target A appearing
# twice and target B once is the deliberate legacy-equivalent replay oracle.
# It proves accepted work is not lost while documenting that source retry can
# duplicate a branch already submitted before the later failure. A second,
# nonmatching source message provides an additional deterministic worker wake.
if [ "$1" != "--case" ]; then
  for fault in bucket entry prepare; do
    RSTB_EGRESS_OOM_CASE=first RSYSLOG_TEST_EGRESS_ALLOC_FAIL=$fault "$0" --case || exit $?
  done
  RSTB_EGRESS_OOM_CASE=late RSYSLOG_TEST_EGRESS_ALLOC_FAIL=prepare:2 "$0" --case || exit $?
  exit 0
fi

. ${srcdir:=.}/diag.sh init
generate_conf
if [ "$RSTB_EGRESS_OOM_CASE" = "late" ]; then
  add_conf '
global(executionEngine="reservedBatch")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target_a" queue.type="LinkedList" queue.size="4") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.a" template="outfmt")
}
ruleset(name="target_b" queue.type="LinkedList" queue.size="4") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'.b" template="outfmt")
}
if $msg contains "msgnum:00000000" then {
  call target_a
  call target_b
}
'
else
  add_conf '
global(executionEngine="reservedBatch")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="2") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum:00000000" then call target
'
fi
startup
injectmsg 0 1
injectmsg 1 1
if [ "$RSTB_EGRESS_OOM_CASE" = "late" ]; then
  wait_file_lines "$RSYSLOG_OUT_LOG.a" 2
  wait_file_lines "$RSYSLOG_OUT_LOG.b" 1
else
  wait_file_lines "$RSYSLOG_OUT_LOG" 1
fi
shutdown_when_empty
wait_shutdown
if [ "$RSTB_EGRESS_OOM_CASE" = "late" ]; then
  content_count_check "msgnum:00000000" 2 "$RSYSLOG_OUT_LOG.a"
  content_count_check "msgnum:00000000" 1 "$RSYSLOG_OUT_LOG.b"
else
  content_count_check "msgnum" 1 "$RSYSLOG_OUT_LOG"
fi
exit_test
