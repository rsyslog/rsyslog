#!/bin/bash
# Exercise real local-wrapper arrays, not a tcpflood sequence of singleton
# submissions. The imdiag testbench fixture submits counts 0, 1, 4, 2, and 5
# through qqueueLocalSubmit(). A FIFO holds the singleton after FE acquisition,
# so F=4 makes the next four-entry array fit, the two-entry array take the
# whole-batch no-fit BE path, and the five-entry array take the oversize BE
# path. Impstats checks the exact FE and no-fit/oversize counters; the fixture
# snapshot baselines after the blocked singleton establishes FE startup, then
# proves all 12 supplied references reached one terminal action exactly once.
# Explicit helperBatchSize=0 preserves the same-binary S2 control.
# Disable internal-message ingress: debug builds can emit BE-worker startup
# diagnostics after the fixture baseline, polluting its exact batch counters.
# File and counter waits establish ordering; no elapsed delay is a success
# condition.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin impstats
require_plugin omtesting
require_plugin imdiag

export NUMMESSAGES=12
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ENTERFILE="$PWD/${RSYSLOG_DYNNAME}.entered"
RELEASEFIFO="$PWD/${RSYSLOG_DYNNAME}.release"
mkfifo "$RELEASEFIFO"

generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/omtesting/.libs/omtesting")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="3"
	queue.local.helperBatchSize="0" queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTERFILE' '$RELEASEFIFO';localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup

response=$(printf "localqueuesubmittest prime\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
    *"OK expected.messages=12"*) ;;
    *) echo "FAIL: unexpected prime response: $response"; error_exit 1 ;;
esac
wait_file_lines "$ENTERFILE" 1

response=$(printf "localqueuesubmittest batches\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
    *"OK expected.messages=12 fe.messages=5 be.messages=7"*) ;;
    *) echo "FAIL: unexpected batch response: $response"; error_exit 1 ;;
esac
localq_wait_stats "$STATSFILE" "main Q.local" \
	"route.fe.messages=5" "route.fe.batches=2" \
	"route.be.reason.nofit.messages=2" "route.be.reason.oversized.messages=5"
localq_wait_stats "$STATSFILE" "main Q.local.frontend.1" \
	"admitted.messages=5" "batch.submit.count=4" "batch.submit.messages.max=5" \
	"overflow.messages=7" "overflow.nofit.messages=2" "overflow.oversized.messages=5" \
	"overflow.batches=2" "overflow.oversized_batches=1"

localq_release_barrier "$RELEASEFIFO"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
localq_wait_stats "$STATSFILE" "main Q.local" \
	"route.fe.messages=5" "route.fe.batches=2" \
	"route.be.reason.nofit.messages=2" "route.be.reason.oversized.messages=5"
response=$(printf "localqueuesubmittest snapshot\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
    *"attempts=12 admitted=12 terminal=12 outstanding=0 fe=5 be=7 be_nofit=2 be_oversized=5"*) ;;
    *) echo "FAIL: unexpected terminal snapshot: $response"; error_exit 1 ;;
esac

shutdown_when_empty
wait_shutdown
seq_check
exit_test
