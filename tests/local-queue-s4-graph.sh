#!/bin/bash
# A reverse-declared local ruleset graph fans out to a local queued action and
# another local ruleset. Each asynchronous submission captures the pre-mutation
# message, while a synchronous call retains the upstream worker. Exact IDs and
# phase values in all three sinks prove copy semantics and independent delivery;
# positive FE routing in every local destination proves downstream registration.
# Waits observe output/stats, not scheduling delays. No throughput claim is made.
. ${srcdir:=.}/diag.sh init
. "$srcdir/local-queue-common.sh"
require_plugin imtcp
require_plugin impstats
export NUMMESSAGES=2001
STATSFILE="$PWD/$RSYSLOG_DYNNAME.stats"
ACTIONFILE="$PWD/$RSYSLOG_DYNNAME.action"
LEAFFILE="$PWD/$RSYSLOG_DYNNAME.leaf"
NEXT_RULESET=leaf
MIDDLE_RULESET=''
MAIN_QUEUE=''
if [ "${LOCAL_QUEUE_S4_DIRECT_MAIN:-0}" -eq 1 ]; then MAIN_QUEUE='main_queue(queue.type="Direct")'; fi
if [ "${LOCAL_QUEUE_S4_GLOBAL_BOUNDARY:-0}" -eq 1 ]; then
    NEXT_RULESET=middle
    MIDDLE_RULESET='ruleset(name="middle" queue.type="FixedArray" queue.size="4096"
        queue.workerThreads="2" queue.workerThreadMinimumMessages="1") { call leaf }'
fi
generate_conf
localq_make_startup_marker_absolute
add_conf '
global(processInternalMessages="off" abortOnUncleanConfig="on")
'"$MAIN_QUEUE"'
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" log.syslog="off" interval="1")
template(name="snapshot" type="string" string="%msg:F,58:2% %$!phase%\n")
ruleset(name="leaf" queue.type="FixedArray" queue.scope="local" queue.size="4096"
    queue.workerThreads="1" queue.dequeueBatchSize="32"
    queue.local.frontendSize="128" queue.local.maxFrontends="8") {
    action(type="omfile" file="'$LEAFFILE'" template="snapshot")
}
'"$MIDDLE_RULESET"'
ruleset(name="sync") {
    action(name="child" type="omfile" file="'$ACTIONFILE'" template="snapshot"
        action.copyMsg="on" queue.type="FixedArray" queue.scope="local" queue.size="4096"
        queue.workerThreads="1" queue.dequeueBatchSize="32"
        queue.local.frontendSize="128" queue.local.maxFrontends="8")
}
ruleset(name="root" queue.type="FixedArray" queue.scope="local" queue.size="4096"
    queue.workerThreads="2" queue.workerThreadMinimumMessages="1" queue.dequeueBatchSize="32"
    queue.local.frontendSize="128" queue.local.maxFrontends="2") {
    if ($msg contains "msgnum:") then {
        set $!phase = "before";
        call sync
        call '"$NEXT_RULESET"'
        set $!phase = "after";
        action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="snapshot")
    }
}
input(type="imtcp" address="127.0.0.1" port="0" ruleset="root" workerThreads="2"
    listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
'
startup
# One initial message guarantees a fitting submission at every boundary before
# the concurrent burst; FE registration is not inferred from connection count.
tcpflood -m1
wait_file_lines "$RSYSLOG_OUT_LOG" 1
tcpflood -m"$((NUMMESSAGES - 1))" -i1 -c2 -Y
for sink in "$RSYSLOG_OUT_LOG" "$ACTIONFILE" "$LEAFFILE"; do
    wait_file_lines --abort-on-oversize "$sink" "$NUMMESSAGES"
done
for queue in root leaf "child queue"; do
    localq_wait_stats_regex "$STATSFILE" "$queue.local" 'route.fe.messages=[1-9][0-9]*'
done
shutdown_when_empty
wait_shutdown
python3 - "$NUMMESSAGES" "$RSYSLOG_OUT_LOG" "$ACTIONFILE" "$LEAFFILE" <<'PY'
import sys
from pathlib import Path
expected = list(range(int(sys.argv[1])))
for filename, phase in zip(sys.argv[2:], ('after', 'before', 'before')):
    rows = [line.split() for line in Path(filename).read_text().splitlines()]
    assert all(len(row) == 2 and row[1] == phase for row in rows), filename
    assert sorted(int(row[0]) for row in rows) == expected, filename
PY
if [ "$?" -ne 0 ]; then error_exit 1; fi
exit_test
