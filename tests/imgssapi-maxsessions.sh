#!/bin/bash
# Verify that imgssapi forwards its configured max session count to tcpsrv.
. ${srcdir:=.}/diag.sh init
require_plugin imgssapi
skip_platform "FreeBSD" "This test currently does not work on FreeBSD"

export NUMMESSAGES=500
MAXSESSIONS=10
CONNECTIONS=20
EXPECTED_DROPS=$((CONNECTIONS - MAXSESSIONS))
MIN_EXPECTED_DROPS=$((EXPECTED_DROPS - 2))
EXPECTED_STR='too many tcp sessions - dropping incoming request'

wait_too_many_sessions()
{
	local count
	count=$(grep "$EXPECTED_STR" "$RSYSLOG_OUT_LOG" | wc -l)
	test "$count" -ge "$MIN_EXPECTED_DROPS" && test "$count" -le "$EXPECTED_DROPS"
}

export QUEUE_EMPTY_CHECK_FUNC=wait_too_many_sessions

generate_conf
add_conf '
$MaxMessageSize 10k
module(load="../plugins/imgssapi/.libs/imgssapi")
$InputGSSServerPermitPlainTCP on
$InputGSSListenPortFileName '$RSYSLOG_DYNNAME'.gss_port
$InputGSSServerMaxSessions '$MAXSESSIONS'
$InputGSSServerRun 0
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
startup

assign_file_content GSS_PORT "$RSYSLOG_DYNNAME.gss_port"
export TCPFLOOD_PORT="$GSS_PORT"
tcpflood -c"$CONNECTIONS" -m"$NUMMESSAGES" -r -d100 -P129 -A

shutdown_when_empty
wait_shutdown

count=$(grep "$EXPECTED_STR" "$RSYSLOG_OUT_LOG" | wc -l)
if [ "$count" -lt "$MIN_EXPECTED_DROPS" ] || [ "$count" -gt "$EXPECTED_DROPS" ]; then
	echo "FAIL: expected between $MIN_EXPECTED_DROPS and $EXPECTED_DROPS dropped sessions, got $count"
	exit 1
fi
exit_test
