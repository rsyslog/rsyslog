#!/bin/bash
# Verify controlled INTERNAL_MSG records enter BE. One actual imtcp producer
# first holds an FE callback at omtesting's two-callback barrier. Only after
# that FE lease is observed does imdiag submit the second message directly to
# BE, where the dedicated worker releases the barrier. Each callback emits one
# controlled internal diagnostic. Exact normal/internal output counts, one FE
# admission, and a BE internal-route increase of at least two prove the routes.
# File/counter predicates establish the phases; a connection count is never
# assumed to imply independent producers, and timeout is only a hang watchdog.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
INTERNAL_OUT="$PWD/${RSYSLOG_DYNNAME}.internal"
NORMAL_OUT="$PWD/${RSYSLOG_DYNNAME}.normal"

generate_conf
localq_make_startup_marker_absolute
add_conf '
template(name="localqid" type="string" string="%msg:F,58:2%\n")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
	queue.local.frontendSize="8" queue.local.maxFrontends="2" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg%\n")
if ($msg contains "msgnum:") then :omtesting:barrier_error 2 ;localqfmt
if ($msg contains "msgnum:") then
	action(type="omfile" file="'$NORMAL_OUT'" template="localqid" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
if ($msg contains "omtesting synchronized error") then
	action(type="omfile" file="'$INTERNAL_OUT'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup
tcpflood -m1 -i0
localq_wait_stats "$STATSFILE" "main Q.local" \
    "fe.registered=1" "fe.inflight.messages=1" "fe.queued.messages=0"
before_internal=$(grep -F "main Q.local: origin=core.queue.local " "$STATSFILE" | tail -n 1 | \
    sed -n 's/.* route.be.reason.internal.messages=\([0-9][0-9]*\).*/\1/p')
case "$before_internal" in ''|*[!0-9]*) error_exit 1 'missing internal-route baseline' ;; esac

# The first callback cannot consume this unclassified imdiag submission from
# its own FE ring. Its BE worker is a distinct execution context by construction.
injectmsg 1 1
wait_file_lines --abort-on-oversize "$NORMAL_OUT" 2
wait_file_lines --abort-on-oversize "$INTERNAL_OUT" 2
localq_wait_stats_greater "$STATSFILE" "main Q.local" \
    route.be.reason.internal.messages "$((before_internal + 1))"
localq_wait_stats "$STATSFILE" "main Q.local" "route.fe.messages=1"
shutdown_when_empty
wait_shutdown
export NUMMESSAGES=2
export SEQ_CHECK_FILE="$NORMAL_OUT"
seq_check
exit_test
