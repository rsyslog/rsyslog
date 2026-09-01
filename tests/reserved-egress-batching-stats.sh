#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Prove the no-pressure path retains target batching without using queue-side
# minimum-batch waiting. A test-only worker gate holds all source work while 32
# messages are submitted; after the gate is removed, one nonmatching release
# message triggers ordinary worker advice. The source dequeue ceiling is large
# enough to claim startup messages, the 32-message stimulus, and the release as
# one immediately available batch. Exact delivery proves all 96 branches were
# published; impstats must report one publication batch and one advice.
. ${srcdir:=.}/diag.sh init
export STATSFILE="$RSYSLOG_DYNNAME.stats"
export RSYSLOG_TEST_EGRESS_WORKER_GATE="$RSYSLOG_DYNNAME.egress-worker-gate"
: > "$RSYSLOG_TEST_EGRESS_WORKER_GATE"
generate_conf
add_conf '
global(executionEngine="reservedBatch")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off"
       resetCounters="off" log.file="'$STATSFILE'")
main_queue(queue.type="LinkedList" queue.dequeueBatchSize="1024")
template(name="outfmt" type="string" string="%msg%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="128") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then {
  call target
  call target
  set $.target_name = "target";
  call_indirect $.target_name;
}
'
startup
injectmsg 0 32
rm -f "$RSYSLOG_TEST_EGRESS_WORKER_GATE"
injectmsg_literal "egress worker release"
wait_file_lines "$RSYSLOG_OUT_LOG" 96

i=0
while ! grep -E 'target: origin=core.queue .*egress\.published\.messages=96 ' \
    "$STATSFILE" >/dev/null 2>&1; do
  i=$((i + 1))
  if [ "$i" -ge 50 ]; then
    echo "expected reserved-egress publication counters for 96 normal-path branches"
    test -f "$STATSFILE" && tail -20 "$STATSFILE"
    error_exit 1
  fi
  "$TESTTOOL_DIR/msleep" 100
done

stats_line=$(grep -E 'target: origin=core.queue .*egress\.published\.messages=96 ' "$STATSFILE" | tail -1)
published_batches=$(printf '%s\n' "$stats_line" | sed -n 's/.*egress\.published\.batches=\([0-9][0-9]*\).*/\1/p')
publication_advice=$(printf '%s\n' "$stats_line" | sed -n 's/.*egress\.publication\.advice=\([0-9][0-9]*\).*/\1/p')
if [ "$published_batches" != 1 ] || [ "$publication_advice" != 1 ]; then
  echo "expected one publication batch/advice for 96 normal-path branches: $stats_line"
  error_exit 1
fi

shutdown_when_empty
wait_shutdown
content_count_check "msgnum" 96 "$RSYSLOG_OUT_LOG"
exit_test
