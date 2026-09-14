#!/bin/bash
# Verify shutdown cancels a blocked FE callback safely and routes an imdiag
# diagnostic injection to BE as an unclassified submission.  The test waits for
# the FIFO-enter marker, injects a distinct unclassified payload, then requests immediate shutdown without
# releasing the callback.  Proper termination plus the pre-shutdown snapshot are
# the oracle: this intentionally does not infer omfile delivery from an
# interrupted transaction commit.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
ENTERFILE="$PWD/${RSYSLOG_DYNNAME}.entered"
RELEASEFIFO="$PWD/${RSYSLOG_DYNNAME}.release"
mkfifo "$RELEASEFIFO"

generate_conf
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="16"
	queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="2"
	queue.timeoutShutdown="1" queue.timeoutActionCompletion="1"
	queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg%\n")
if ($msg contains "localq-block") then :omtesting:file_barrier '$ENTERFILE' '$RELEASEFIFO';localqfmt
if ($msg contains "localq-") then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt" queue.type="Direct"
		asyncWriting="off" flushOnTXEnd="on")
'
startup
tcpflood -m1 -M'localq-block'
wait_file_lines "$ENTERFILE" 1
# imdiag has no trusted imtcp producer tag. Its distinct literal must enter BE,
# and the FE count must remain one while the callback is held.
injectmsg_literal '<167>Mar  1 01:00:00 host tag localq-internal'
localq_wait_stats "$STATSFILE" "main Q.local" \
	"fe.registered=1" "route.be.reason.unclassified.messages=1" "fe.inflight.messages=1"
shutdown_immediate
wait_shutdown
exit_test
