#!/bin/bash
# One dedicated BE worker is held on ID0. After an actual single imtcp producer
# starts and empties FE1, imdiag submits ID2 to BE. Its callback can run only on
# the helper. While that borrowed callback is held, publish local ID3 and BE
# IDs4..5. H=1 requires the helper to finish ID2, prefer ID3, then acquire each
# remaining BE message. The default-cap wrapper uses H=3 and two remaining
# messages. Its FE batch-count increase of one and message-sum increase of two
# prove one immediately available partial batch. Exact output order proves
# source priority; inventory and full shutdown conservation prove no extra
# admission or transfer accounting.
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
input(type="imtcp" address="127.0.0.1" port="0"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" workerThreads="1")
main_queue(queue.scope="local" queue.type="'${LOCAL_QUEUE_TEST_BE_TYPE:-FixedArray}'" queue.size="'$be_capacity'"
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
    # The after-registration gate holds FE, so any BE producer may own BE
    # while waiting to deliver its signal. An internal diagnostic can arrive
    # before imdiag; wait for pending BE inventory rather than requiring one
    # particular producer to acquire that already-held mutex. The dedicated
    # callback fixes active holdings until the later BE_RELEASE phase.
    # Keep the imdiag producer owned and join it after external gate release.
    injectmsg 2 1 &
    inject_pid=$!
    localq_wait_stats_greater "$STATSFILE" "main Q.local" be.physical.messages be.active.messages
    localq_release_barrier "$GATE_RELEASE"
    wait "$inject_pid" || error_exit 1
else
    localq_wait_stats_regex "$STATSFILE" "main Q.local.frontend.1" 'help.waits=[1-9][0-9]*' 'inflight.fe=0'
    injectmsg 2 1
fi
wait_file_lines "$HELP_ENTER" 1
localq_wait_stats "$STATSFILE" "main Q.local" 'inflight.help=1' 'be.active.messages=2' "batch.help.limit=$help_limit"
if [ "$help_limit" -eq 3 ]; then
    # Synchronize on the FE record itself before sampling. The aggregate main
    # record may have advanced while the frontend impstats record is older.
    help_before=$(localq_wait_stats "$STATSFILE" "main Q.local.frontend.1" \
        'inflight.help=1' 'batch.help.limit=3')
    help_count_before=$(printf '%s\n' "$help_before" | sed -n 's/.* batch.help.count=\([0-9][0-9]*\).*/\1/p')
    help_sum_before=$(printf '%s\n' "$help_before" | sed -n 's/.* batch.help.messages.sum=\([0-9][0-9]*\).*/\1/p')
    case "$help_count_before" in
        ''|*[!0-9]*) error_exit 1 'missing baseline helper batch count' ;;
    esac
    case "$help_sum_before" in
        ''|*[!0-9]*) error_exit 1 'missing baseline helper batch message sum' ;;
    esac
fi
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
    # impstats includes startup INTERNAL_MSG work, so its cumulative batch
    # counters need a baseline. IDs4/5 are the only remaining BE work. One
    # added helper batch carrying two messages proves their immediate borrow
    # is partial under D=3, while the lifetime maximum remains bounded by D.
    help_count_expected=$((help_count_before + 1))
    help_sum_expected=$((help_sum_before + 2))
    help_after=$(localq_wait_stats "$STATSFILE" "main Q.local.frontend.1" \
        'inflight.help=0' 'batch.help.limit=3' \
        "batch.help.count=$help_count_expected" "batch.help.messages.sum=$help_sum_expected")
    help_count_after=$(printf '%s\n' "$help_after" | sed -n 's/.* batch.help.count=\([0-9][0-9]*\).*/\1/p')
    help_sum_after=$(printf '%s\n' "$help_after" | sed -n 's/.* batch.help.messages.sum=\([0-9][0-9]*\).*/\1/p')
    help_max_after=$(printf '%s\n' "$help_after" | sed -n 's/.* batch.help.messages.max=\([0-9][0-9]*\).*/\1/p')
    case "$help_count_after" in
        ''|*[!0-9]*) error_exit 1 'missing final helper batch count' ;;
    esac
    case "$help_sum_after" in
        ''|*[!0-9]*) error_exit 1 'missing final helper batch message sum' ;;
    esac
    case "$help_max_after" in
        ''|*[!0-9]*) error_exit 1 'missing final helper batch maximum' ;;
    esac
    [ "$((help_count_after - help_count_before))" -eq 1 ] ||
        error_exit 1 "expected one final helper batch, saw $help_count_before -> $help_count_after"
    [ "$((help_sum_after - help_sum_before))" -eq 2 ] ||
        error_exit 1 "expected partial two-message helper batch, saw $help_sum_before -> $help_sum_after"
    [ "$help_max_after" -le "$help_limit" ] ||
        error_exit 1 "helper batch maximum exceeded $help_limit: $help_max_after"
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
