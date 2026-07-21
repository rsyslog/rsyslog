#!/bin/bash
## Verify that imptcp attaches the accepted peer's numeric source port to
## every message. The tcpflood port file is obtained with getsockname(), and
## exact comparison after synchronized shutdown proves property correctness.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=4
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" address="127.0.0.1" port="0"
      listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port")
template(name="outfmt" type="string" string="%fromhost-port%\n")
if $msg contains "msgnum:" then
  action(type="omfile" template="outfmt" file="'"$RSYSLOG_OUT_LOG"'")
'
startup
tcpflood -p"$(cat "${RSYSLOG_DYNNAME}.tcpflood_port")" -m"$NUMMESSAGES" \
	-w "${RSYSLOG_DYNNAME}.tcpflood-source-port"
shutdown_when_empty
wait_shutdown
SOURCE_PORT=$(cat "${RSYSLOG_DYNNAME}.tcpflood-source-port")
EXPECTED=$(printf '%s\n%s\n%s\n%s' "$SOURCE_PORT" "$SOURCE_PORT" "$SOURCE_PORT" "$SOURCE_PORT")
export EXPECTED
cmp_exact
exit_test
