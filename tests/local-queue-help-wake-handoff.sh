#!/bin/bash
# FE1 uses the existing imdiag actual-producer fixture, FE2 an independent
# single imtcp worker. A dedicated BE callback is blocked. The test-only gate
# holds selected FE2 before its source recheck; its producer publishes local
# work, which also blocks. FE1 must receive the wake handoff and process BE ID2
# while both FE2 and the dedicated BE worker are blocked. This detects backlog
# stranded by removing a selected helper from the idle list without retaining
# its responsibility to pass the wake. Actual callback markers establish the
# interleaving; no connection is used as a producer identity.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
BE_ENTRY="$PWD/${RSYSLOG_DYNNAME}.be.entry"
BE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.be.release"
FE_ENTRY="$PWD/${RSYSLOG_DYNNAME}.fe.entry"
FE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.fe.release"
GATE_ENTRY="$PWD/${RSYSLOG_DYNNAME}.gate.entry"
GATE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.gate.release"
STOPMARK="$PWD/${RSYSLOG_DYNNAME}.stop"
mkfifo "$BE_RELEASE" "$FE_RELEASE" "$GATE_RELEASE"
export RSYSLOG_LOCAL_QUEUE_TEST_HELP_GATE=selected
export RSYSLOG_LOCAL_QUEUE_TEST_HELP_ENTRY="$GATE_ENTRY"
export RSYSLOG_LOCAL_QUEUE_TEST_HELP_RELEASE="$GATE_RELEASE"
generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="64" queue.workerThreads="1"
 queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
 queue.local.frontendSize="4" queue.local.maxFrontends="2" queue.local.frontendStats="on")
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000100:") then :omtesting:file_barrier '$BE_ENTRY' '$BE_RELEASE';localqfmt
if ($msg contains "msgnum:00000003:") then :omtesting:file_barrier '$FE_ENTRY' '$FE_RELEASE';localqfmt
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt"
 queue.type="Direct" asyncWriting="off" flushOnTXEnd="on")
'
startup
injectmsg 100 1
wait_file_lines "$BE_ENTRY" 1
response=$(printf 'localqueuesubmittest prime\n' | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in *OK*) ;; *) error_exit 1 ;; esac
localq_wait_stats_regex "$STATSFILE" 'main Q.local.frontend.1' 'help.waits=[1-9][0-9]*'
tcpflood -m1 -i1
localq_wait_stats_regex "$STATSFILE" 'main Q.local.frontend.2' 'help.waits=[1-9][0-9]*'
injectmsg 2 1
wait_file_lines "$GATE_ENTRY" 1
tcpflood -m1 -i3
localq_wait_stats "$STATSFILE" 'main Q.local.frontend.2' 'queued=1'
localq_release_barrier "$GATE_RELEASE"
wait_file_lines "$FE_ENTRY" 1
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3
# ID2 must finish on FE1 while FE2's local ID3 and the dedicated ID100 are held.
# shellcheck disable=SC2034
EXPECTED=$'00000000\n00000001\n00000002'
cmp_exact "$RSYSLOG_OUT_LOG"
localq_wait_stats_regex "$STATSFILE" 'main Q.local.frontend.1' 'terminal.help=[1-9][0-9]*'
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
case "$response" in *OK*) ;; *) error_exit 1 ;; esac
localq_release_barrier "$FE_RELEASE"
localq_release_barrier "$BE_RELEASE"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 5
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
exit_test
