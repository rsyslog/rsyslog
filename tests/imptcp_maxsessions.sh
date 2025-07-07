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
wait_too_many_sessions()
{
  test "$(grep "$EXPECTED_STR" "$RSYSLOG_OUT_LOG" | wc -l)" = "$EXPECTED_DROPS"
}

export QUEUE_EMPTY_CHECK_FUNC=wait_too_many_sessions
generate_conf
add_conf '
$MaxMessageSize 10k

module(load="../plugins/imptcp/.libs/imptcp" maxsessions="'$MAXSESSIONS'")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
'
startup

echo "INFO: RSYSLOG_OUT_LOG: $RSYSLOG_OUT_LOG"

echo "About to run tcpflood"
tcpflood -c$CONNECTIONS -m$NUMMESSAGES -r -d100 -P129 -A
echo "-------> NOTE: CLOSED REMOTELY messages are expected and OK! <-------"
echo "done run tcpflood"
shutdown_when_empty
wait_shutdown

content_count_check "$EXPECTED_STR" $EXPECTED_DROPS
echo "Got expected drops: $EXPECTED_DROPS, looks good!"

exit_test
