#!/bin/bash
# Test imtcp with many dropping connections
# added 2010-08-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test currently does not work on FreeBSD"
export NUMMESSAGES=500

MAXSESSIONS=10
CONNECTIONS=20
EXPECTED_DROPS=$((CONNECTIONS - MAXSESSIONS))

EXPECTED_STR='too many tcp sessions - dropping incoming request'
STARTED_LOG="${RSYSLOG_DYNNAME}.started"
HOLDER_PID=

stop_holder()
{
	if [ -n "$HOLDER_PID" ]; then
		if kill -0 "$HOLDER_PID" 2>/dev/null; then
			kill "$HOLDER_PID" 2>/dev/null || true
		fi
		wait "$HOLDER_PID" 2>/dev/null || true
		HOLDER_PID=
	fi
}

test_error_exit_handler()
{
	stop_holder
}

count_dropped_sessions()
{
	if [ -f "$STARTED_LOG" ]; then
		count=$(grep -F -c -- "$EXPECTED_STR" "$STARTED_LOG" || true)
		case "$count" in
			''|*[!0-9]*) printf '0\n' ;;
			*) printf '%s\n' "$count" ;;
		esac
	else
		printf '0\n'
	fi
}

wait_for_exact_drops()
{
	wait_tries=${1:-20}
	wait_i=1

	while [ "$wait_i" -le "$wait_tries" ]; do
		count=$(count_dropped_sessions)
		if [ "$count" -eq "$EXPECTED_DROPS" ]; then
			return 0
		fi
		if [ "$count" -gt "$EXPECTED_DROPS" ]; then
			break
		fi
		$TESTTOOL_DIR/msleep 1000
		wait_i=$((wait_i + 1))
	done

	return 1
}

start_holder()
{
	./tcpflood -p"$TCPFLOOD_PORT" -c"$MAXSESSIONS" -m"$MAXSESSIONS" -P129 -K0 &
	HOLDER_PID=$!
}

generate_conf
add_conf '
$MaxMessageSize 10k

module(load="../plugins/imptcp/.libs/imptcp" maxsessions="'$MAXSESSIONS'")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:2%\n"

if $msg contains "msgnum:" then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}

$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
'
startup

echo "INFO: RSYSLOG_OUT_LOG: $RSYSLOG_OUT_LOG"

echo "Opening $MAXSESSIONS held sessions"
start_holder
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$MAXSESSIONS"

echo "About to run tcpflood for $EXPECTED_DROPS extra sessions"
tcpflood -c$EXPECTED_DROPS -m$EXPECTED_DROPS -i$MAXSESSIONS -P129 -A
echo "-------> NOTE: CLOSED REMOTELY messages are expected and OK! <-------"
echo "done run tcpflood"

if ! wait_for_exact_drops 20; then
	count=$(count_dropped_sessions)
	echo "FAIL: expected exactly $EXPECTED_DROPS dropped sessions, got $count"
	echo "STARTED_LOG contents:"
	if [ -f "$STARTED_LOG" ]; then
		cat "$STARTED_LOG"
	else
		echo "$STARTED_LOG does not exist"
	fi
	error_exit 1
fi

wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$MAXSESSIONS" 2
stop_holder
shutdown_when_empty
wait_shutdown

count=$(count_dropped_sessions)
if [ "$count" -ne "$EXPECTED_DROPS" ]; then
	echo "FAIL: expected exactly $EXPECTED_DROPS dropped sessions, got $count"
	echo "STARTED_LOG contents:"
	if [ -f "$STARTED_LOG" ]; then
		cat "$STARTED_LOG"
	else
		echo "$STARTED_LOG does not exist"
	fi
	exit 1
fi
seq_check 0 $((MAXSESSIONS - 1))
echo "Got expected drops: $EXPECTED_DROPS, looks good!"

exit_test
