#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Preserve known-success COMM after a real callback/Direct omfile commit,
# through FE/BE cooperative immediate stop and cancellation. A test-only
# post-return gate certifies the successful ID0 callback before shutdown starts.
# ID1 is queued behind it; final conservation and recovery must retain that
# pending obligation too. SaveOnShutdown permits unresolved work to survive.
# After ungated restart, ID2 completes; exact ID/attempt inventory 0:1,1:1,2:1
# proves no avoidable replay or repeated script mutation. This is a controlled
# at-least-once fixture, not an exactly-once delivery claim. Interrupted Direct
# commit replay remains covered by local-queue-force-term-actions.sh and its
# BE/helper wrappers. Phase/entry markers and queue stats synchronize the test;
# deadlines only force the selected stop/cancel path and protect against hangs.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin imdiag
require_plugin impstats

for source in fe be; do
    for stop in cooperative cancel; do
        CASE="$PWD/$RSYSLOG_DYNNAME.$source-$stop"
        SPOOL="$CASE.spool"
        STATS="$CASE.stats"
        ENTRY="$CASE.entry"
        RELEASE="$CASE.release"
        STOPMARK="$CASE.stop"
        mkdir -p "$SPOOL"
        mkfifo "$RELEASE"
        export RSYSLOG_LOCAL_QUEUE_TEST_SUCCESS_GATE="$source"
        export RSYSLOG_LOCAL_QUEUE_TEST_SUCCESS_ENTRY="$ENTRY"
        export RSYSLOG_LOCAL_QUEUE_TEST_SUCCESS_RELEASE="$RELEASE"
        action_timeout=10000
        [ "$stop" != cancel ] || action_timeout=100

        generate_conf
        export RSYSLOG_OUT_LOG="$CASE.output"
        localq_make_startup_marker_absolute
        add_conf '
global(workDirectory="'$SPOOL'" processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATS'")
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
main_queue(queue.scope="local" queue.type="FixedArray" queue.filename="known-success"
 queue.size="32" queue.highWatermark="24" queue.lowWatermark="8"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="1"
 queue.timeoutShutdown="1" queue.timeoutActionCompletion="'$action_timeout'" queue.saveOnShutdown="on"
 queue.local.frontendSize="4" queue.local.maxFrontends="1" queue.local.helperBatchSize="0")
template(name="ids" type="string" string="%msg:F,58:2%:%$!attempt%\n")
if ($msg contains "msgnum:") then {
 set $!attempt = $!attempt + 1;
 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids" queue.type="Direct"
  asyncWriting="off" flushOnTXEnd="on")
}
'
        startup
        # Finish all startup work before admitting the gated data callback.
        wait_queueempty
        response=$(printf 'localqueuestopcheck %s\n' "$STOPMARK" | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT") || error_exit $?
        case "$response" in *OK*) ;; *) error_exit 1 'success stop-check arm failed' ;; esac
        if [ "$source" = fe ]; then
            tcpflood -m1 -i0
        else
            injectmsg 0 1
        fi
        wait_file_lines "$ENTRY" 1
        custom_content_check "OK source=$source committed=1" "$ENTRY"
        wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1
        if [ "$source" = fe ]; then
            tcpflood -m1 -i1
            localq_wait_stats "$STATS" 'main Q.local' 'route.fe.messages=2' \
                'fe.inflight.messages=1' 'fe.queued.messages=1' 'be.active.messages=0'
        else
            injectmsg 1 1
            localq_wait_stats "$STATS" 'main Q.local' 'fe.registered=0' \
                'be.active.messages=1' 'be.physical.messages=2'
        fi
        shutdown_immediate
        wait_file_lines "${STOPMARK}.action-phase" 1
        if [ "$stop" = cooperative ]; then
            localq_release_barrier "$RELEASE"
        fi
        wait_shutdown
        wait_file_lines "$STOPMARK" 1
        # The gate has already written ID0. Its final disposition must be
        # executed, never a retained/persisted/discarded ambiguous obligation.
        custom_content_check 'outstanding=0 admitted=2 terminal=2 rejected=0' "$STOPMARK"
        custom_content_check 'discarded=0' "$STOPMARK"
        executed=$(sed -n 's/.* executed=\([0-9][0-9]*\).*/\1/p' "$STOPMARK")
        persisted=$(sed -n 's/.* persisted=\([0-9][0-9]*\).*/\1/p' "$STOPMARK")
        case "$executed:$persisted" in *[!0-9:]*|:*|*:) error_exit 1 'missing success outcomes' ;; esac
        [ "$executed" -ge 1 ] && [ "$((executed + persisted))" -eq 2 ] || \
            error_exit 1 'known-success or queued obligation was lost'
        content_count_check --regex '^00000000:1$' 1
        unset RSYSLOG_LOCAL_QUEUE_TEST_SUCCESS_GATE RSYSLOG_LOCAL_QUEUE_TEST_SUCCESS_ENTRY RSYSLOG_LOCAL_QUEUE_TEST_SUCCESS_RELEASE
        startup
        injectmsg 2 1
        wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 3
        shutdown_when_empty
        wait_shutdown
        sort "$RSYSLOG_OUT_LOG" > "$CASE.sorted"
        # cmp_exact reads this intentional global fixture value.
        # shellcheck disable=SC2034
        EXPECTED=$'00000000:1\n00000001:1\n00000002:1'
        cmp_exact "$CASE.sorted"
    done
done
exit_test
