#!/bin/bash
# add 2016-11-22 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv4.bits="8" ipv4.mode="simple")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: 1.1.1.8
<129>Mar 10 01:00:00 172.20.245.8 tag: 0.0.0.0
<129>Mar 10 01:00:00 172.20.245.8 tag: 172.0.234.255
<129>Mar 10 01:00:00 172.20.245.8 tag: 111.1.1.8.\""

shutdown_when_empty
wait_shutdown
export EXPECTED=' 1.1.1.x
 0.0.0.x
 172.0.234.xxx
 111.1.1.x.'
cmp_exact
exit_test
