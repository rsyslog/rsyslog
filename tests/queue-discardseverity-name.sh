#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A global ruleset queue configured with a named warning threshold must retain
# a critical message while its worker is blocked, then discard warnings at the
# watermark. The Direct main queue parses priority before handing messages to
# the ruleset queue. The barrier, queue-size snapshots, and a pre-release
# discard counter sample order admission. Exact output IDs and the final
# discard counter also prove dequeue-time discard of a warning that entered
# below the watermark.
# Polling is only for observable state; the testbench timeout guards hangs.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin impstats
require_plugin omtesting

STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
ENTRY="$PWD/$RSYSLOG_DYNNAME.entry"
RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
mkfifo "$RELEASE"
exec {release_fd}<>"$RELEASE"

wait_policy_size() {
    local expected="$1" deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) line
    while [ "$(date +%s)" -le "$deadline" ]; do
        line=$(grep -F 'policy: origin=core.queue ' "$STATSFILE" 2>/dev/null | tail -n 1 || true)
        case " $line " in
            *" size=$expected "*) return 0 ;;
        esac
        "$TESTTOOL_DIR/msleep" 100
    done
    error_exit 1 "policy queue did not reach size=$expected; last stats: $line"
}

generate_conf
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'$STATSFILE'")
module(load="../plugins/omtesting/.libs/omtesting")
main_queue(queue.type="Direct")
template(name="ids" type="string" string="%msg:F,58:2%\n")
ruleset(name="policy" queue.type="FixedArray" queue.size="8"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
 queue.dequeueBatchSize="1" queue.discardMark="1" queue.discardSeverity="warning") {
 if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '$ENTRY' '$RELEASE';ids
 if ($msg contains "msgnum:") then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="ids")
}
input(type="imtcp" address="127.0.0.1" port="0" workerThreads="1"
 listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if ($msg contains "msgnum:") then call policy
'
startup
tcpflood -m1 -i0 -P162
wait_file_lines "$ENTRY" 1
tcpflood -m1 -i1 -P164
wait_policy_size 1
tcpflood -m1 -i2 -P162
wait_policy_size 2
tcpflood -m1 -i3 -P164
wait_content 'policy: origin=core.queue .*discarded.nf=1' "$STATSFILE"
printf 'release\n' >&"$release_fd"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 2
wait_content 'policy: origin=core.queue .*discarded.nf=2' "$STATSFILE"
shutdown_when_empty
wait_shutdown
printf '00000000\n00000002\n' > "$RSYSLOG_DYNNAME.expected"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG"
exec {release_fd}>&-
exit_test
