#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# addd 2016-05-13 by RGerhards, released under ASL 2.0
# An oversized LF-framed message is truncated without changing the following
# message, and oversizemsg.report="off" suppresses its diagnostic. Checking the
# configured internal-message output after shutdown is the silence oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on" oversizemsg.report="off")
module(load="../plugins/imtcp/.libs/imtcp" discardTruncatedMsg="on")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset1")

template(name="outfmt" type="string" string="%rawmsg%\n")
ruleset(name="ruleset1") {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
action(type="omfile" file="'$RSYSLOG2_OUT_LOG'")
'
startup
tcpflood -m1 -M "\"<30>Apr  6 13:21:08 docker/6befb258da22[6128]: TOO LONG bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb123456789B123456789Ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
<30>Apr  6 13:21:09 docker/6defd258da22[6128]: NEXT_MSG\""
shutdown_when_empty
wait_shutdown

export EXPECTED='<30>Apr  6 13:21:08 docker/6befb258da22[6128]: TOO LONG bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb123456789B12
<30>Apr  6 13:21:09 docker/6defd258da22[6128]: NEXT_MSG'
cmp_exact
if grep -q 'received oversize delimiter-framed message' "$RSYSLOG2_OUT_LOG"; then
	echo "FAIL: oversize diagnostic emitted with oversizemsg.report=off"
	cat "$RSYSLOG2_OUT_LOG"
	error_exit 1
fi

exit_test
