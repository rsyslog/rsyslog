#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test for issue #4960: raw UDP messages with critical PRI values
# must not be discarded by main-queue discardSeverity before the PRI is parsed.
# A file barrier holds the sole main-queue worker on ID 0. While it is held,
# the queue size includes ID 0: its stats snapshot is 1. Emergency PRI-0 filler
# ID 1 arrives below discardMark and raises the queue size to 2. Critical PRI-2
# ID 2 then arrives while the pre-enqueue size is exactly discardMark;
# size=3 and discarded.nf=0 prove the message passed the pre-parse discard check
# remains queued. A second UDP input uses pmnull configured to rewrite messages
# to debug: its raw PRI-2 ID 3 must still be discarded using the stored severity,
# because plugin parsers can replace PRI after the core's automatic parsing.
# For that check, size=4 with discarded.nf=0 is an immediate failure oracle:
# it means raw PRI was trusted before pmnull could rewrite it.
# Out-of-range legacy PRI ID 4 (192, parsed as invalid) and no-PRI ID 5 (default
# severity 5) then verify existing discard behavior. discarded.nf=3 proves all
# three checks completed and output omits them. Dynamic ports prove both
# listeners are bound, the barrier fixes occupancy, and these post-check
# counters—not the arrival counter—are the success oracle; the testbench timeout
# is only a hang guard. Compressed legacy-z messages are intentionally outside
# this regression: their pre-parse severity remains conservative until normal
# parsing after queue admission.
. ${srcdir:=.}/diag.sh init
require_plugin imudp
require_plugin impstats
require_plugin omtesting
require_plugin pmnull
check_command_available python3

STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
PORT_RCVR_FILE="$RSYSLOG_DYNNAME.udp_port"
PORT_PMNULL_FILE="$RSYSLOG_DYNNAME.pmnull_udp_port"
ENTRY="$PWD/$RSYSLOG_DYNNAME.entry"
RELEASE="$PWD/$RSYSLOG_DYNNAME.release"
mkfifo "$RELEASE"
exec {release_fd}<>"$RELEASE"

wait_main_queue_size() {
    local expected="$1" deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) line
    while [ "$(date +%s)" -le "$deadline" ]; do
        line=$(grep -F 'main Q: origin=core.queue ' "$STATSFILE" 2>/dev/null | tail -n 1 || true)
        case " $line " in
            *" size=$expected "*)
                MAIN_QUEUE_STATS_LINE="$line"
                return 0
                ;;
        esac
        "$TESTTOOL_DIR/msleep" 100
    done
    error_exit 1 "main queue did not reach size=$expected; last stats: $line"
}

wait_main_queue_discarded() {
    local expected="$1" deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) line
    while [ "$(date +%s)" -le "$deadline" ]; do
        line=$(grep -F 'main Q: origin=core.queue ' "$STATSFILE" 2>/dev/null | tail -n 1 || true)
        case " $line " in
            *" discarded.nf=$expected "*)
                MAIN_QUEUE_STATS_LINE="$line"
                return 0
                ;;
        esac
        "$TESTTOOL_DIR/msleep" 100
    done
    error_exit 1 "main queue did not report discarded.nf=$expected; last stats: $line"
}

wait_pmnull_fallback() {
    local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) line
    while [ "$(date +%s)" -le "$deadline" ]; do
        line=$(grep -F 'main Q: origin=core.queue ' "$STATSFILE" 2>/dev/null | tail -n 1 || true)
        case " $line " in
            *" discarded.nf=1 "*)
                MAIN_QUEUE_STATS_LINE="$line"
                return 0
                ;;
            *" size=4 "*" discarded.nf=0 "*)
                error_exit 1 "main queue trusted raw PRI before the pmnull severity rewrite; stats: $line"
                ;;
        esac
        "$TESTTOOL_DIR/msleep" 100
    done
    error_exit 1 "main queue did not apply conservative pmnull severity fallback; last stats: $line"
}

wait_critical_priority_admitted() {
    local deadline=$(( $(date +%s) + TB_TEST_TIMEOUT )) line
    while [ "$(date +%s)" -le "$deadline" ]; do
        line=$(grep -F 'main Q: origin=core.queue ' "$STATSFILE" 2>/dev/null | tail -n 1 || true)
        case " $line " in
            *" size=3 "*" discarded.nf=0 "*)
                MAIN_QUEUE_STATS_LINE="$line"
                return 0
                ;;
            *" discarded.nf=1 "*)
                error_exit 1 "main queue discarded a critical UDP message at discardMark; stats: $line"
                ;;
        esac
        "$TESTTOOL_DIR/msleep" 100
    done
    error_exit 1 "critical UDP message was not admitted at discardMark; last stats: $line"
}

generate_conf
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
module(load="../plugins/imudp/.libs/imudp")
module(load="../plugins/impstats/.libs/impstats" interval="1" log.syslog="off" log.file="'"$STATSFILE"'")
module(load="../plugins/omtesting/.libs/omtesting")
module(load="../plugins/pmnull/.libs/pmnull")
parser(name="fixed.debug" type="pmnull" syslogseverity="debug")
template(name="ids" type="string" string="%msg:F,58:2%\n")
main_queue(queue.type="FixedArray" queue.size="8"
 queue.workerThreads="1" queue.workerThreadMinimumMessages="1"
 queue.dequeueBatchSize="1" queue.discardMark="2" queue.discardSeverity="warning")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'"$PORT_RCVR_FILE"'")
ruleset(name="pmnull_ruleset" parser="fixed.debug") {
 if ($msg contains "msgnum:") then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="ids")
}
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'"$PORT_PMNULL_FILE"'" ruleset="pmnull_ruleset")
if ($msg contains "msgnum:00000000:") then :omtesting:file_barrier '"$ENTRY"' '"$RELEASE"';ids
if ($msg contains "msgnum:") then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="ids")
'
startup
assign_file_content PORT_RCVR "$PORT_RCVR_FILE"
assign_file_content PORT_PMNULL "$PORT_PMNULL_FILE"

tcpflood -Tudp -p"$PORT_RCVR" -m1 -i0 -P0
wait_file_lines "$ENTRY" 1
wait_main_queue_size 1

tcpflood -Tudp -p"$PORT_RCVR" -m1 -i1 -P0
wait_main_queue_size 2
tcpflood -Tudp -p"$PORT_RCVR" -m1 -i2 -P2
wait_critical_priority_admitted
GOOD_PRIORITY_STATS_LINE="$MAIN_QUEUE_STATS_LINE"

tcpflood -Tudp -p"$PORT_PMNULL" -m1 -i3 -P2
wait_pmnull_fallback
tcpflood -Tudp -p"$PORT_RCVR" -m1 -i4 -P192
wait_main_queue_discarded 2
export PORT_RCVR
python3 - <<'PY'
import os
import socket

message = b"Mar  1 01:00:00 testhost tag msgnum:00000005:"
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
    udp_socket.sendto(message, ("127.0.0.1", int(os.environ["PORT_RCVR"])))
PY
wait_main_queue_discarded 3

printf 'release\n' >&"$release_fd"
shutdown_when_empty
wait_shutdown

case " $GOOD_PRIORITY_STATS_LINE " in
    *" discarded.nf=0 "*) ;;
    *) error_exit 1 "main queue discarded a critical UDP message before/at PRI parsing; stats: $GOOD_PRIORITY_STATS_LINE" ;;
esac
case " $MAIN_QUEUE_STATS_LINE " in
    *" discarded.nf=3 "*) ;;
    *) error_exit 1 "main queue did not discard exactly the malformed/no-PRI messages; stats: $MAIN_QUEUE_STATS_LINE" ;;
esac
printf '00000000\n00000001\n00000002\n' > "$RSYSLOG_DYNNAME.expected"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG"
exec {release_fd}>&-
exit_test
