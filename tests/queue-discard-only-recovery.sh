#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A debug message admitted below the discard watermark reaches it at dequeue,
# leaving a batch with no deliverable elements but one physical queue record.
# Both memory queue types must retire that record before the worker goes idle.
# After observing the expected enqueue/discard counts, a fresh impstats snapshot
# with size=0 proves retirement before further input triggers another dequeue.
# Waiting for a later snapshot avoids mixing a pre-enqueue size with newer
# counters, since impstats reads individual counters separately. Critical
# messages must then be delivered without restarting; exact output and repeated
# cycles check that capacity is neither retained nor retired twice. Polling
# waits for observable state; the testbench timeout only guards a stuck test.
if [ -z "${RSYSLOG_TESTBENCH_DISCARD_QUEUE_TYPE:-}" ]; then
	RSYSLOG_TESTBENCH_DISCARD_QUEUE_TYPE=LinkedList "$0" || exit $?
	RSYSLOG_TESTBENCH_DISCARD_QUEUE_TYPE=FixedArray "$0" || exit $?
	exit 0
fi
. ${srcdir:=.}/diag.sh init
require_plugin impstats

STATS_FILE="$PWD/$RSYSLOG_DYNNAME.stats"

wait_retired() {
	local enqueued="$1" discarded="$2" first_line deadline
	wait_content "main Q: origin=core.queue .*enqueued=$enqueued .*discarded.nf=$discarded " "$STATS_FILE"
	first_line=$(( $(wc -l < "$STATS_FILE") + 1 ))
	deadline=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	while ! tail -n "+$first_line" "$STATS_FILE" |
		grep -q "main Q: origin=core.queue size=0 enqueued=$enqueued .*discarded.nf=$discarded "; do
		if [ "$(date +%s)" -ge "$deadline" ]; then
			error_exit 1 "queue did not retire after enqueued=$enqueued discarded.nf=$discarded"
		fi
		"$TESTTOOL_DIR/msleep" 100
	done
}

generate_conf
add_conf '
global(processInternalMessages="off")
module(load="../plugins/impstats/.libs/impstats" interval="1"
    log.syslog="off" log.file="'"$STATS_FILE"'")
main_queue(queue.type="'"$RSYSLOG_TESTBENCH_DISCARD_QUEUE_TYPE"'" queue.size="8"
    queue.workerThreads="1" queue.dequeueBatchSize="1"
    queue.discardMark="1" queue.discardSeverity="7")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:") then
    action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
'

startup
for cycle in 1 2 3; do
	injectmsg 0 1
	wait_retired "$((2 * cycle - 1))" "$cycle"
	injectmsg_literal "<162>Jan  1 00:00:00 localhost test: msgnum:$cycle:"
	wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$cycle"
	wait_retired "$((2 * cycle))" "$cycle"
done
shutdown_when_empty
wait_shutdown
export EXPECTED=$'1\n2\n3'
cmp_exact
exit_test
