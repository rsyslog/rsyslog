#!/bin/bash
# Reconcile every exported local-queue counter after real whole-batch
# submissions, then verify impstats resetCounters leaves the adapter's
# CTR_FLAG_NONE lifetime snapshot unchanged. The initialized-daemon fixture
# submits 0, 1, 4, 2 and 5 element arrays: one blocked FE owns 5 messages and
# the whole 2/5 arrays use BE for no-fit/oversize. Exact IDs and the fixture's
# native snapshot establish quiescence; two completed impstats samples carrying
# identical totals prove a resettable scrape neither clears nor double-counts
# logical or per-FE counters. Polling only waits for those observable records.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

export NUMMESSAGES=12
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ENTERFILE="$PWD/${RSYSLOG_DYNNAME}.entered"
RELEASEFIFO="$PWD/${RSYSLOG_DYNNAME}.release"
mkfifo "$RELEASEFIFO"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1" resetCounters="on")
module(load="../plugins/omtesting/.libs/omtesting")
main_queue(queue.scope="local" queue.type="'${LOCAL_QUEUE_TEST_BE_TYPE:-FixedArray}'" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="3"
	queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTERFILE' '$RELEASEFIFO';localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup

response=$(printf "localqueuesubmittest prime\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue submit prime response: $response"
	error_exit 1
fi
wait_file_lines "$ENTERFILE" 1

response=$(printf "localqueuesubmittest batches\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
if [[ "$response" != *"OK"* ]]; then
	echo "FAIL: local queue submit batches response: $response"
	error_exit 1
fi
# The initialized daemon has startup INTERNAL_MSGs. The fixture snapshot is a
# post-startup delta, so it is the exact logical 12/7/5 oracle. Raw impstats
# still proves the FE and whole-batch overflow fields after one resettable scrape.
# Submission acknowledges BE admission, not completion. Poll its terminal
# state while FE remains held; the watchdog only bounds a failed drain.
deadline=$((SECONDS + 60))
while :; do
response=$(printf "localqueuesubmittest snapshot\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
	*"attempts=12"*"admitted=12"*"terminal=7"*"outstanding=5"*"fe=5"*"be=7"*"be_nofit=2"*"be_oversized=5"*) break ;;
	*) ;;
esac
    (( SECONDS < deadline )) || error_exit 1 "BE snapshot did not settle: $response"
    $TESTTOOL_DIR/msleep 100
done
localq_wait_stats "$STATSFILE" "main Q.local" \
	"route.fe.messages=5" "route.fe.batches=2" "route.be.reason.nofit.messages=2" \
	"route.be.reason.oversized.messages=5" "outstanding.messages=5"
localq_wait_stats "$STATSFILE" "main Q.local.frontend.1" \
	"admitted.messages=5" "admitted.batches=2" "batch.submit.count=4" "batch.submit.messages.max=5" \
	"overflow.messages=7" "overflow.nofit.messages=2" "overflow.oversized.messages=5" \
	"overflow.batches=2" "overflow.oversized_batches=1"

localq_release_barrier "$RELEASEFIFO"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
response=$(printf "localqueuesubmittest snapshot\n" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in
	*"attempts=12"*"admitted=12"*"terminal=12"*"outstanding=0"*"fe=5"*"be=7"*"be_nofit=2"*"be_oversized=5"*) ;;
	*) echo "FAIL: local queue submit snapshot response: $response"; error_exit 1 ;;
esac

# Repeated impstats records after resetCounters=on must retain the same
# baseline-independent local counters. The final native delta above already
# reconciles the target logical totals without assuming startup-message counts.
localq_wait_stable_stats "$STATSFILE" "main Q.local" 2 \
	"outstanding.messages=0" "route.fe.messages=5" "route.fe.batches=2" \
	"route.be.reason.nofit.messages=2" "route.be.reason.oversized.messages=5" \
	"be.physical.messages=0" "be.active.messages=0" "fe.queued.messages=0" "fe.inflight.messages=0" "fe.retry.messages=0"
localq_wait_stable_stats "$STATSFILE" "main Q.local.frontend.1" 2 \
	"admitted.messages=5" "admitted.batches=2" "batch.submit.count=4" "batch.submit.messages.max=5" \
	"terminal.messages=5" "overflow.messages=7" "overflow.nofit.messages=2" \
	"overflow.oversized.messages=5" "overflow.batches=2" "overflow.oversized_batches=1" \
	"queued=0" "inflight.fe=0" "retry.fe=0"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
