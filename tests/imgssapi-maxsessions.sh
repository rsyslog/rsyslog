#!/bin/bash
# Verify that imgssapi forwards its configured max session count to tcpsrv.
. ${srcdir:=.}/diag.sh init
require_plugin imgssapi
skip_platform "FreeBSD" "This test currently does not work on FreeBSD"

export NUMMESSAGES=500
MAXSESSIONS=10
CONNECTIONS=20
EXPECTED_DROPS=$((CONNECTIONS - MAXSESSIONS))
# This test only needs to prove that the configured cap is enforced at all.
# Under slower or coverage-instrumented builds, accepted sessions may churn
# while tcpflood is still opening later sockets, so the exact drop count is
# not stable.
MIN_EXPECTED_DROPS=1
EXPECTED_STR='too many tcp sessions - dropping incoming request'
STARTED_LOG="${RSYSLOG_DYNNAME}.started"

count_dropped_sessions()
{
	if [ -f "$STARTED_LOG" ]; then
		grep -F -c -- "$EXPECTED_STR" "$STARTED_LOG" || true
	else
		printf '0\n'
	fi
}

wait_for_drop_logs()
{
	wait_tries=${1:-15}
	wait_i=1

	while [ "$wait_i" -le "$wait_tries" ]; do
		count=$(count_dropped_sessions)
		if [ "$count" -ge "$MIN_EXPECTED_DROPS" ] && [ "$count" -le "$EXPECTED_DROPS" ]; then
			return 0
		fi
		$TESTTOOL_DIR/msleep 1000
		wait_i=$((wait_i + 1))
	done

	return 1
}

generate_conf
add_conf '
$MaxMessageSize 10k
module(load="../plugins/imgssapi/.libs/imgssapi")
$InputGSSServerPermitPlainTCP on
$InputGSSServerTokenIOTimeout 10
$InputGSSListenPortFileName '$RSYSLOG_DYNNAME'.gss_port
$InputGSSServerMaxSessions '$MAXSESSIONS'
$InputGSSServerRun 0
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
startup

assign_file_content GSS_PORT "$RSYSLOG_DYNNAME.gss_port"
export TCPFLOOD_PORT="$GSS_PORT"
tcpflood -c"$CONNECTIONS" -m"$NUMMESSAGES" -r -d100 -P129 -A

# Wait briefly for rsyslog's own session-drop diagnostics to land, but do not
# hold shutdown open on an exact drop count. That makes the test hang on slow
# coverage runners even when the main queue is already empty.
wait_for_drop_logs 15 || true
shutdown_when_empty
wait_shutdown

count=$(count_dropped_sessions)
if [ "$count" -lt "$MIN_EXPECTED_DROPS" ] || [ "$count" -gt "$EXPECTED_DROPS" ]; then
	echo "FAIL: expected between $MIN_EXPECTED_DROPS and $EXPECTED_DROPS dropped sessions, got $count"
	echo "STARTED_LOG contents:"
	cat "$STARTED_LOG"
	exit 1
fi
exit_test
