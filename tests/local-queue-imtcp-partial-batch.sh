#!/bin/bash
# Verify an FE consumes an exact partial final batch.  The first callback is
# held by a FIFO, then five queued IDs are released into a dequeue ceiling of
# three.  The per-FE snapshot must report three batches and six messages
# (1 + 3 + 2); exact output IDs after release prove the retained suffix was
# neither overwritten nor replayed.  Marker/file waits, not elapsed time,
# establish every phase. The S6 wrapper also checks minimum-batch timeout
# for an isolated singleton, then clamping of the requested minimum to FE size.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting

export NUMMESSAGES=6
fe_capacity=5
batch_size=3
queued_messages=5
min_config=''
if [ "${LOCAL_QUEUE_S6_MIN_BATCH:-0}" = 1 ]; then
    NUMMESSAGES=4
    fe_capacity=3
    batch_size=8
    queued_messages=3
    min_config='queue.minDequeueBatchSize="5" queue.minDequeueBatchSize.timeout="1000"'
fi
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ENTERFILE="$PWD/${RSYSLOG_DYNNAME}.entered"
RELEASEFIFO="$PWD/${RSYSLOG_DYNNAME}.release"
mkfifo "$RELEASEFIFO"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="'$batch_size'" '"$min_config"'
	queue.local.frontendSize="'$fe_capacity'" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTERFILE' '$RELEASEFIFO';localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup
tcpflood -m1 -i0
wait_file_lines "$ENTERFILE" 1
tcpflood -m"$queued_messages" -i1
localq_wait_stats "$STATSFILE" "main Q.local" "route.fe.messages=$NUMMESSAGES" "fe.queued.messages=$queued_messages"
localq_release_barrier "$RELEASEFIFO"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
if [ "${LOCAL_QUEUE_S6_MIN_BATCH:-0}" = 1 ]; then
    # The singleton arrived alone and reached its callback only through the
    # minimum timeout. The next three fill F, below requested minimum5: it
    # must clamp to D=3 and acquire all three in one batch, never overrun FE.
    localq_wait_stats "$STATSFILE" "main Q.local.frontend.1" \
        "batch.fe_dequeue.count=2" "batch.fe_dequeue.messages.sum=4" "batch.fe_dequeue.messages.max=3"
else
    localq_wait_stats "$STATSFILE" "main Q.local.frontend.1" \
        "batch.fe_dequeue.count=3" "batch.fe_dequeue.messages.sum=6" "batch.fe_dequeue.messages.max=3"
fi
shutdown_when_empty
wait_shutdown
seq_check
exit_test
