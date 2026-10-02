#!/bin/bash
# Stop a reverse-declared graph while the root BE callback owns ID1 and the
# downstream BE is full. Independent leaf FE/BE barriers establish backlog;
# the graph-entry marker proves shutdown started before any callback release.
# All six IDs must survive: downstream queues must remain available until the
# root has submitted its held message. The normal shutdown watchdog detects
# deadlock; no sleep or elapsed-time threshold is a success oracle.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=6
STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
ROOT_ENTER="$PWD/$RSYSLOG_DYNNAME.root-enter"
ROOT_RELEASE="$PWD/$RSYSLOG_DYNNAME.root-release"
FE_ENTER="$PWD/$RSYSLOG_DYNNAME.fe-enter"
FE_RELEASE="$PWD/$RSYSLOG_DYNNAME.fe-release"
BE_ENTER="$PWD/$RSYSLOG_DYNNAME.be-enter"
BE_RELEASE="$PWD/$RSYSLOG_DYNNAME.be-release"
export RSYSLOG_LOCAL_QUEUE_TEST_GRAPH_SHUTDOWN_FILE="$PWD/$RSYSLOG_DYNNAME.graph-stop"
mkfifo "$ROOT_RELEASE" "$FE_RELEASE" "$BE_RELEASE"
# Keep open() out of the callback's signal window: graceful worker shutdown
# sends a wake signal. The barrier read retries EINTR, whereas FIFO open does
# not. Owned RDWR handles also ensure a failed callback cannot hang the shell.
exec {root_release_fd}<>"$ROOT_RELEASE"
exec {fe_release_fd}<>"$FE_RELEASE"
exec {be_release_fd}<>"$BE_RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/omtesting/.libs/omtesting")
template(name="ids" type="string" string="%msg:F,58:2%\n")
ruleset(name="leaf" queue.type="FixedArray" queue.scope="local" queue.size="3"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="10000" queue.timeoutActionCompletion="1000"
    queue.local.frontendSize="1" queue.local.maxFrontends="2" queue.local.helperBatchSize="0") {
    if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$FE_ENTER' '$FE_RELEASE';ids
    if ($msg contains "msgnum:00000003:") then :omtesting:file_barrier '$BE_ENTER' '$BE_RELEASE';ids
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
}
main_queue(queue.type="FixedArray" queue.scope="local" queue.size="32"
    queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
    queue.timeoutShutdown="10000" queue.timeoutActionCompletion="1000"
    queue.local.frontendSize="8" queue.local.maxFrontends="1" queue.local.helperBatchSize="0")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if ($msg contains "msgnum:") then {
    if ($msg contains "msgnum:00000001:") then :omtesting:file_barrier '$ROOT_ENTER' '$ROOT_RELEASE';ids
    call leaf
}
'
startup
tcpflood -m1 -i0
wait_file_lines "$FE_ENTER" 1
injectmsg 1 1
wait_file_lines "$ROOT_ENTER" 1
tcpflood -m4 -i2
wait_file_lines "$BE_ENTER" 1
localq_wait_stats "$STATSFILE" "leaf.local" "fe.queued.messages=1" "be.physical.messages=3"
shutdown_immediate
wait_file_exists "$RSYSLOG_LOCAL_QUEUE_TEST_GRAPH_SHUTDOWN_FILE"
printf 'release\n' >&"$fe_release_fd"
printf 'release\n' >&"$be_release_fd"
printf 'release\n' >&"$root_release_fd"
wait_shutdown
exec {root_release_fd}>&-
exec {fe_release_fd}>&-
exec {be_release_fd}>&-
seq_check
exit_test
