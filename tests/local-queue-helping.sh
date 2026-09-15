#!/bin/bash
# One dedicated BE worker is held on ID0. After an actual single imtcp producer
# starts and empties FE1, imdiag submits ID2 to BE. Its callback can run only on
# the helper. While that borrowed callback is held, publish local ID3 and BE
# IDs4..5. H=1 requires the helper to finish ID2, prefer ID3, then acquire each
# remaining BE message. The default-cap wrapper uses H=3 and two remaining
# messages, proving an immediately available partial batch. Exact output order proves source priority; inventory
# and full shutdown conservation prove no extra admission/transfer accounting.
# FIFO and impstats predicates establish ordering. Timeouts only detect hangs.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
require_plugin omtesting
export NUMMESSAGES=6
help_config='queue.local.helperBatchSize="1"'
help_limit=1
be_capacity=${LOCAL_QUEUE_HELP_BE_CAPACITY:-64}
if [ "${LOCAL_QUEUE_HELP_DEFAULT:-0}" -eq 1 ]; then help_config=''; help_limit=3; fi
STATSFILE="$PWD/${RSYSLOG_DYNNAME}.stats"
BE_ENTER="$PWD/${RSYSLOG_DYNNAME}.be.enter"
BE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.be.release"
HELP_ENTER="$PWD/${RSYSLOG_DYNNAME}.help.enter"
HELP_RELEASE="$PWD/${RSYSLOG_DYNNAME}.help.release"
STOPMARK="$PWD/${RSYSLOG_DYNNAME}.stop"
mkfifo "$BE_RELEASE" "$HELP_RELEASE"
if [ -n "${LOCAL_QUEUE_HELP_GATE:-}" ]; then
    GATE_ENTRY="$PWD/${RSYSLOG_DYNNAME}.gate.enter"
    GATE_RELEASE="$PWD/${RSYSLOG_DYNNAME}.gate.release"
    mkfifo "$GATE_RELEASE"
    export RSYSLOG_LOCAL_QUEUE_TEST_HELP_GATE="$LOCAL_QUEUE_HELP_GATE"
    export RSYSLOG_LOCAL_QUEUE_TEST_HELP_ENTRY="$GATE_ENTRY"
    export RSYSLOG_LOCAL_QUEUE_TEST_HELP_RELEASE="$GATE_RELEASE"
fi
generate_conf
localq_make_startup_marker_absolute
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="FixedArray" queue.size="'$be_capacity'"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="3"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.frontendStats="on"
 '"$help_config"')
template(name="localqfmt" type="string" string="%msg:F,58:2%\n")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$BE_ENTER' '$BE_RELEASE';localqfmt
if ($msg contains "msgnum:00000002:") then :omtesting:file_barrier '$HELP_ENTER' '$HELP_RELEASE';localqfmt
if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="localqfmt"
 queue.type="Direct" asyncWriting="off" flushOnTXEnd="on")
'
startup
injectmsg 0 1
wait_file_lines "$BE_ENTER" 1
tcpflood -m1 -i1
wait_file_lines "$RSYSLOG_OUT_LOG" 1
if [ -n "${LOCAL_QUEUE_HELP_GATE:-}" ]; then
    wait_file_lines "$GATE_ENTRY" 1
    # The after-registration gate holds FE, so admission may block delivering
    # its signal. Keep this producer owned and join it after external release.
    injectmsg 2 1 &
    inject_pid=$!
    localq_wait_stats "$STATSFILE" "main Q.local" 'route.be.reason.unclassified.messages=2'
    localq_release_barrier "$GATE_RELEASE"
    wait "$inject_pid" || error_exit 1
else
    localq_wait_stats_regex "$STATSFILE" "main Q.local.frontend.1" 'help.waits=[1-9][0-9]*' 'inflight.fe=0'
    injectmsg 2 1
fi
wait_file_lines "$HELP_ENTER" 1
localq_wait_stats "$STATSFILE" "main Q.local" 'inflight.help=1' 'be.active.messages=2' "batch.help.limit=$help_limit"
tcpflood -m1 -i3
if [ "$be_capacity" -eq 3 ]; then
    # The dedicated and borrowed active entries occupy two of B=3 slots.
    # The two-message producer must block after its accepted first prefix.
    injectmsg 4 2 &
    capacity_pid=$!
    localq_wait_stats "$STATSFILE" 'main Q.local' 'be.physical.messages=3'
else
    injectmsg 4 2
fi
localq_wait_stats "$STATSFILE" "main Q.local" 'fe.queued.messages=1' 'inflight.help=1'
localq_release_barrier "$HELP_RELEASE"
if [ "$be_capacity" -eq 3 ]; then wait "$capacity_pid" || error_exit 1; fi
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 5
# shellcheck disable=SC2034 # cmp_exact reads the harness global dynamically.
EXPECTED=$'00000001\n00000002\n00000003\n00000004\n00000005'
cmp_exact "$RSYSLOG_OUT_LOG"
localq_wait_stats "$STATSFILE" "main Q.local" 'inflight.help=0' 'transfer.fe_to_be.messages=0'
if [ "$help_limit" -eq 3 ]; then
    localq_wait_stats "$STATSFILE" 'main Q.local.frontend.1' 'batch.help.messages.max=2' 'batch.help.messages.sum=3'
fi
response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")
case "$response" in *OK*) ;; *) error_exit 1 ;; esac
localq_release_barrier "$BE_RELEASE"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 6
shutdown_when_empty
wait_shutdown
wait_file_lines "$STOPMARK" 1
seq_check
exit_test
