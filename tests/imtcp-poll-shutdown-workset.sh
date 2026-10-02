#!/bin/bash
# The poll backend must stop when shutdown is observed inside workset dispatch.
# A testbench-only synthetic readiness event pauses after the outer shutdown
# check. Its marker acknowledges that exact position; SIGTERM then selects the
# inner FORCE_TERM branch after consuming the real input-stop signal. No real
# socket or stop signal remains ready, so retrying poll would hang.
# Proper termination within the watchdog proves the branch exits normally;
# the timeout is failure protection, not a timing or delivery assertion.
. ${srcdir:=.}/diag.sh init
export RSYSLOG_TEST_POLL_SHUTDOWN_MARKER="$PWD/$RSYSLOG_DYNNAME.poll-ready"

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
wait_file_exists "$RSYSLOG_TEST_POLL_SHUTDOWN_MARKER"
shutdown_immediate
wait_shutdown "" 15
exit_test
