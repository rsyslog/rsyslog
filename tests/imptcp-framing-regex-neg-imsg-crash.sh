#!/bin/bash
# Regression test for imptcp regex-framing negative iMsg crash.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. "${srcdir:=.}"/diag.sh init

MAXMSG=256

generate_conf
add_conf '
global(maxMessageSize="'$MAXMSG'")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
    framing.delimiter.regex="^X")
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME".tcpflood_port

# Phase 1: 2*MAXMSG-1 bytes of 'A' + '\n' triggers the oversize handler on the
# 512th byte, resetting iMsg=0/iCurrLine=1; the '\n' branch then sets iCurrLine=0.
# Phase 2: 'X' matches "^X" at iCurrLine=0, computing iMsg = 0 - 1 = -1 (crash).
# Phase 3: follow-up messages verify rsyslog keeps delivering after the attack.
{ head -c $((MAXMSG * 2 - 1)) /dev/zero | tr '\0' 'A'
  printf '\nX\nXtest message 1\nXtest message 2\n'
} > "$RSYSLOG_DYNNAME".crash_payload

tcpflood -B -I "$RSYSLOG_DYNNAME".crash_payload

./msleep 500

PID=$(cat "$RSYSLOG_PIDBASE".pid 2>/dev/null)
if [ -z "$PID" ]; then
    printf 'FAIL: could not read rsyslog PID\n'
    error_exit 1
fi

if ! kill -0 "$PID" 2>/dev/null; then
    printf 'FAIL: rsyslogd crashed after receiving the crafted payload\n'
    error_exit 1
fi

shutdown_when_empty
wait_shutdown
content_check "Xtest message 1"
exit_test
