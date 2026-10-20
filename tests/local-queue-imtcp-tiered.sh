#!/bin/bash
# Verify whole-batch local routing with one actual imtcp worker.  A file/FIFO
# barrier holds the first FE callback and a second holds the first BE callback.
# While both are observable, the test fills a four-slot FE, overflows a five-ID
# batch to BE, releases FE capacity, and submits a fitting batch again.  The
# impstats snapshots prove FE and BE inventory/routes; exact IDs after both
# releases prove no loss or duplicate delivery.  No connection is treated as a
# producer identity: workerThreads=1 constrains the actual execution context.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting

export NUMMESSAGES=12
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
FE_ENTER="$PWD/${RSYSLOG_DYNNAME}.fe.enter"
FE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.fe.release"
BE_ENTER="$PWD/${RSYSLOG_DYNNAME}.be.enter"
BE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.be.release"
mkfifo "$FE_RELEASE" "$BE_RELEASE"

generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="'${LOCAL_QUEUE_TEST_BE_TYPE:-FixedArray}'" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="3"
	queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$FE_ENTER' '$FE_RELEASE';localqfmt
if ($msg contains "msgnum:00000005:") then :omtesting:file_barrier '$BE_ENTER' '$BE_RELEASE';localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup

tcpflood -m1 -i0
wait_file_lines "$FE_ENTER" 1
# Record BE startup traffic only after the first FE exists. The two route
# assertions below use a delta, so rsyslog's own startup diagnostics cannot
# masquerade as the five test messages.
localq_wait_stats "$STATSFILE" "main Q.local" "route.fe.messages=1" "fe.registered=1"
be_baseline=$(grep -F "main Q.local: origin=core.queue.local " "$STATSFILE" | tail -n 1 | \
	sed -n 's/.* route.be.messages=\([0-9][0-9]*\).*/\1/p')
case "$be_baseline" in
	''|*[!0-9]*) echo "FAIL: unable to read local BE baseline: $be_baseline"; error_exit 1 ;;
esac
be_after_overflow=$((be_baseline + 5))
# Four messages fit the blocked FE; the following five-message submission is
# deliberately larger than F and must use the shared BE as one whole batch.
tcpflood -m4 -i1
tcpflood -m5 -i5
wait_file_lines "$BE_ENTER" 1
localq_wait_stats "$STATSFILE" "main Q.local" \
	"route.fe.messages=5" "route.be.messages=$be_after_overflow" "fe.queued.messages=4"

# Releasing only the FE returns four slots while the BE callback remains
# blocked.  A fitting two-ID submission must therefore re-enter FE instead of
# using sticky BE fallback.
localq_release_barrier "$FE_RELEASE"
tcpflood -m2 -i10
localq_wait_stats "$STATSFILE" "main Q.local" \
	"route.fe.messages=7" "route.be.messages=$be_after_overflow"

localq_release_barrier "$BE_RELEASE"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
shutdown_when_empty
wait_shutdown
seq_check
exit_test
