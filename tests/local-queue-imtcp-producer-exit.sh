#!/bin/bash
# Retire the first actual imtcp pool worker after its first FE publication via
# the ENABLE_TESTBENCH-only runtime hook. Its callback remains blocked on a FIFO,
# so producer.exited=1 plus inflight.fe=1 proves a producerless retained FE lease.
# The surviving real worker then submits new IDs. With one lifetime descriptor,
# it must fall back to BE; neither the exited producer's descriptor nor generation
# may be reused. Releasing the retained callback must deliver every ID exactly
# once and allow normal daemon termination. No connection is a producer identity.
# Completion reclaims obligations, not the lifetime registration reservation:
# the exported bound remains 64+1*(8+1)=73 after outstanding reaches zero.
# Registered only with imtcp epoll support: the portable poll implementation
# deliberately forces one thread and therefore cannot exercise pool departure.
# The direct stderr hook marker confirms the actual retirement injection;
# ordinary delivery is checked through the configured omfile output instead.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats
require_plugin omtesting

export RSYSLOG_LOCAL_QUEUE_TEST_FAULT=producer-retire
export RS_REDIR=">${RSYSLOG_DYNNAME}.fault.log 2>&1"
export NUMMESSAGES=17
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
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="2"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.local.frontendSize="8" queue.local.maxFrontends="1" queue.local.frontendStats="on")
template(name="lifecyclefmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then
    :omtesting:file_barrier '$ENTERFILE' '$RELEASEFIFO';lifecyclefmt
if ($msg contains "msgnum:") then
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="lifecyclefmt")
'
startup
tcpflood -m1 -i0
wait_file_lines "$ENTERFILE" 1
localq_wait_stats "$STATSFILE" 'main Q.local' \
    'fe.registered=1' 'fe.started=1' 'fe.producerless=1' 'fe.inflight.messages=1'
content_check 'local queue test fault: producer-retire' "${RSYSLOG_DYNNAME}.fault.log"
localq_wait_stats "$STATSFILE" 'main Q.local.frontend.1' \
    'registration.id=1' 'registration.generation=1' 'producer.exited=1' \
    'inflight.fe=1' 'admitted.messages=1'

# The submitting thread has actually returned through tcpsrv's cleanup handler.
# New work can run only on the other pool worker, while the FE consumer is alive.
tcpflood -m16 -i1
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 16
localq_wait_stats "$STATSFILE" 'main Q.local' \
    'fe.registered=1' 'fe.started=1' 'fe.producerless=1' 'route.fe.messages=1' \
    'route.be.reason.capacity_exhausted.messages=16' 'fe.inflight.messages=1'
localq_wait_stats "$STATSFILE" 'main Q.local.frontend.1' \
    'registration.id=1' 'registration.generation=1' 'producer.exited=1' \
    'inflight.fe=1' 'admitted.messages=1'
localq_release_barrier "$RELEASEFIFO"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$NUMMESSAGES"
localq_wait_stats "$STATSFILE" 'main Q.local' \
    'fe.registered=1' 'fe.producerless=1' 'outstanding.messages=0' \
    'resource.reserved.messages=73' 'resource.be.capacity.messages=64' \
    'resource.fe.capacity.messages=8' 'resource.fe.active.capacity.messages=1'
shutdown_when_empty
wait_shutdown
seq_check
exit_test
