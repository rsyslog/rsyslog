#!/bin/bash
# add 2016-05-13 by RGerhards, released under ASL 2.0

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
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16\""
tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message\""
tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16\""
tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message\""
shutdown_when_empty
wait_shutdown

export EXPECTED='<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 t
<120> 2011-03-01T11:22:12Z host tag: this is a way to long message
<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 t
<120> 2011-03-01T11:22:12Z host tag: this is a way to long message'
cmp_exact
exit_test
