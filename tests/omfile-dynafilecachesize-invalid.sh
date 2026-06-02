#!/bin/bash
# Ensure action-level dynaFileCacheSize values below 1 are normalized instead
# of crashing startup or dynafile writes.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
template(name="dynfile" type="string" string="'$RSYSLOG_DYNNAME'.out.log")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
:msg, contains, "msg:" action(type="omfile" dynafile="dynfile" template="outfmt" dynaFileCacheSize="0")
'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag msg:normalized-cache-size\""
shutdown_when_empty
wait_shutdown

grep "DynaFileCacheSize must be greater 0 (0 given), changed to 1." "${RSYSLOG_DYNNAME}.started" > /dev/null || {
    echo "FAIL: expected normalization message not found in startup log"
    cat "${RSYSLOG_DYNNAME}.started"
    error_exit 1
}

printf 'normalized-cache-size\n' > "${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "${RSYSLOG_DYNNAME}.out.log" || {
    echo "FAIL: dynafile write did not succeed after normalization"
    cat "${RSYSLOG_DYNNAME}.out.log"
    error_exit 1
}

exit_test
