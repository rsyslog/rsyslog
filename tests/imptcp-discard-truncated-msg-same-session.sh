#!/bin/bash
# Regression test for imptcp discardTruncatedMsg recovery within one TCP
# session. The issue reproducer sends an oversized LF-framed message and the
# next message over the same connection; success proves that the first byte
# after truncation is not copied into the next message. The oracle is exact
# output comparison: the second line must start with "<120>", not a leftover
# byte from the truncated first line.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="ruleset1" discardTruncatedMsg="on")

template(name="outfmt" type="string" string="%rawmsg%\n")
ruleset(name="ruleset1") {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
printf '%s\n%s\n' \
	'<120> 2023-07-10T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 test5' \
	'<120> 2023-07-10T11:22:12Z host tag: this is a short msg' \
	> "$RSYSLOG_DYNNAME.input"
tcpflood -B -I "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown

export EXPECTED='<120> 2023-07-10T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 t
<120> 2023-07-10T11:22:12Z host tag: this is a short msg'
cmp_exact
exit_test
