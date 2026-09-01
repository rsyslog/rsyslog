#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Atomically flip a test-only selector override after the first selected element
# of one source batch. A test-only worker gate forms the batch from already
# available work without queue-side minimum-batch waiting; removing it and
# submitting a nonmatching release message takes the ordinary immediate advice
# path. This does not emulate reload. Ordered 0,1 target output proves both
# legacy->reserved and reserved->legacy cases use the selector captured at batch
# entry; rereading it could reorder publication or use an uninitialized ledger.
if [ "$1" != "--case" ]; then
  for engine in legacy reservedBatch; do
    RSYSLOG_TEST_EGRESS_FLIP_AFTER_FIRST=1 RSTB_CASE_ENGINE=$engine "$0" --case || exit $?
  done
  exit 0
fi

. ${srcdir:=.}/diag.sh init
export RSYSLOG_TEST_EGRESS_WORKER_GATE="$RSYSLOG_DYNNAME.egress-worker-gate"
: > "$RSYSLOG_TEST_EGRESS_WORKER_GATE"
generate_conf
add_conf '
global(executionEngine="'$RSTB_CASE_ENGINE'")
main_queue(queue.type="LinkedList" queue.dequeueBatchSize="1024")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="target" queue.type="LinkedList" queue.size="10") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
if $msg contains "msgnum" then call target
'
startup
injectmsg 0 2
rm -f "$RSYSLOG_TEST_EGRESS_WORKER_GATE"
injectmsg_literal "egress worker release"
shutdown_when_empty
wait_shutdown
seq_check 0 1
exit_test
