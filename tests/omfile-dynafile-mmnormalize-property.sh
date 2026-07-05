#!/usr/bin/env bash
# Regression reproducer for issue #3488: mmnormalize populates a JSON property
# used by an omfile dynafile template. The old crash happened while resolving
# that property during action processing. The oracle is clean shutdown and the
# expected dynafile write; any SIGSEGV or failed template lookup fails the test.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="writeData")
template(name="LogFormat" type="string" string="%msg%\n")
template(name="WriteFile" type="string" string="'$RSYSLOG_DYNNAME'.%$!clientIP%.log")

ruleset(name="writeData"
        queue.type="fixedArray"
        queue.size="10000"
        queue.dequeueBatchSize="2048"
        queue.workerThreads="2"
        queue.workerThreadMinimumMessages="10000") {
    action(type="mmnormalize" rule="rule=:<%pri:number%>%clientIP:ipv4% %other:rest%" userawmsg="on")
    if $parsesuccess == "OK" then {
        action(type="omfile" DynaFile="WriteFile" template="LogFormat")
    }
}
'

startup
tcpflood -m1 -O -M "\"<13>192.0.2.15 - - \\\"GET / HTTP/1.1\\\" 200 17 \\\"-\\\" \\\"-\\\" \\\"-\\\"\""
shutdown_when_empty
wait_shutdown

printf ' - "GET / HTTP/1.1" 200 17 "-" "-" "-"\n' > "${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.192.0.2.15.log" || {
    echo "FAIL: dynafile write did not use the normalized clientIP property"
    cat "${RSYSLOG_DYNNAME}.192.0.2.15.log"
    error_exit 1
}

exit_test
