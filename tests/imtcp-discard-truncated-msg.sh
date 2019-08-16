#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on")
module(load="../plugins/imtcp/.libs/imtcp" discardTruncatedMsg="on")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset1")

template(name="outfmt" type="string" string="%rawmsg%\n")
ruleset(name="ruleset1") {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
tcpflood -m1 -M "\"<30>Apr  6 13:21:08 docker/6befb258da22[6128]: TOO LONG bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb123456789B123456789Ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
<30>Apr  6 13:21:09 docker/6defd258da22[6128]: NEXT_MSG\""
shutdown_when_empty
wait_shutdown

export EXPECTED='<30>Apr  6 13:21:08 docker/6befb258da22[6128]: TOO LONG bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb123456789B12
<30>Apr  6 13:21:09 docker/6defd258da22[6128]: NEXT_MSG'
cmp_exact

exit_test
