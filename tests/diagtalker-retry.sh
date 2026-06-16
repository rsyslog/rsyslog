#!/bin/bash
# This test covers diagtalker's connection retry path. minitcpsrv binds an
# ephemeral port and publishes it before listen(), so the port stays reserved
# while diagtalker receives deterministic connection refusals. The retry
# diagnostic proves the retry path ran; the later command file and fixed
# response prove the same diagtalker process completed a real request after the
# listener was released.

. ${srcdir:=.}/diag.sh init

LISTEN_RELEASE="$RSYSLOG_DYNNAME.diagtalker-listen-release"
LISTEN_READY="$RSYSLOG_DYNNAME.diagtalker-listen-ready"
ACCEPT_READY="$RSYSLOG_DYNNAME.diagtalker-accept-ready"
DIAGTALKER_STDOUT="$RSYSLOG_DYNNAME.diagtalker.stdout"
DIAGTALKER_STDERR="$RSYSLOG_DYNNAME.diagtalker.stderr"

export MINITCPSRV_EXTRA_OPTS="-r imdiag::ok"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1 "$LISTEN_RELEASE" "$LISTEN_READY" "$ACCEPT_READY"
unset MINITCPSRV_EXTRA_OPTS

printf 'status\n' | "$TESTTOOL_DIR/diagtalker" -p"$MINITCPSRVR_PORT1" \
	>"$DIAGTALKER_STDOUT" 2>"$DIAGTALKER_STDERR" &
DIAGTALKER_PID=$!

wait_content "connect failed, retrying" "$DIAGTALKER_STDERR"

: > "$LISTEN_RELEASE"
wait_file_exists "$LISTEN_READY"
wait_file_exists "$ACCEPT_READY"

wait "$DIAGTALKER_PID" || error_exit $?

wait_file_lines "$RSYSLOG_OUT_LOG" 1 20
content_check "status" "$RSYSLOG_OUT_LOG"
content_check "connection established" "$DIAGTALKER_STDERR"
content_check "imdiag[$MINITCPSRVR_PORT1]: imdiag::ok" "$DIAGTALKER_STDOUT"

exit_test
