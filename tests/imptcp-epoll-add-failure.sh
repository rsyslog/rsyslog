#!/bin/bash
# This test injects an imptcp session epoll_ctl(ADD) failure after startup.
# The oracle is twofold: rsyslog must log the forced accept failure, proving the
# post-accept error path ran, and then shut down cleanly without walking a
# dangling session list entry left by that failure.
. ${srcdir:=.}/diag.sh init
platform="$(uname)"
if [ "$platform" != "Linux" ]; then
	echo "platform is \"$platform\" - test requires Linux epoll and LD_PRELOAD support"
	skip_test
fi
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
require_plugin imptcp

export NUMMESSAGES=1
export RSYSLOG_TEST_EPOLL_CTL_FAIL_MARKER="${RSYSLOG_DYNNAME}.fail-session-epoll"
export RSYSLOG_PRELOAD=.libs/liboverride_epoll_ctl.so
STARTED_LOG="${RSYSLOG_DYNNAME}.started"
EXPECTED_STR="imptcp: failed to fully accept session"

test_error_exit_handler()
{
	rm -f "$RSYSLOG_TEST_EPOLL_CTL_FAIL_MARKER"
}

wait_for_accept_failure()
{
	wait_i=1
	while [ "$wait_i" -le 30 ]; do
		if [ -f "$STARTED_LOG" ] && grep -F -q -- "$EXPECTED_STR" "$STARTED_LOG"; then
			return 0
		fi
		$TESTTOOL_DIR/msleep 100
		wait_i=$((wait_i + 1))
	done
	return 1
}

generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

: > "$RSYSLOG_TEST_EPOLL_CTL_FAIL_MARKER"
tcpflood -c1 -m1 -P129 -A || true

if ! wait_for_accept_failure; then
	echo "FAIL: did not observe forced imptcp session accept failure"
	if [ -f "$STARTED_LOG" ]; then
		cat "$STARTED_LOG"
	fi
	error_exit 1
fi

rm -f "$RSYSLOG_TEST_EPOLL_CTL_FAIL_MARKER"
shutdown_when_empty
wait_shutdown
exit_test
