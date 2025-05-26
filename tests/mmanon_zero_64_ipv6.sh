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
	action(type="mmanon" ipv6.bits="64" ipv6.anonmode="zero")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: asdfghjk
<129>Mar 10 01:00:00 172.20.245.8 tag: FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
<129>Mar 10 01:00:00 172.20.245.8 tag: 61:34:ad::7:F aa:ff43::756:99:0
<129>Mar 10 01:00:00 172.20.245.8 tag: ::
<129>Mar 10 01:00:00 172.20.245.8 tag: 0::
<129>Mar 10 01:00:00 172.20.245.8 tag: 13:abd:45:
<129>Mar 10 01:00:00 172.20.245.8 tag: textnoblank72:8374:adc7:47FF::43:0:1AFEstillnoblank\""

shutdown_when_empty
wait_shutdown
export EXPECTED=' asdfghjk
 ffff:ffff:ffff:ffff:0:0:0:0
 61:34:ad:0:0:0:0:0 aa:ff43:0:0:0:0:0:0
 0:0:0:0:0:0:0:0
 0:0:0:0:0:0:0:0
 13:abd:45:
 textnoblank72:8374:adc7:47ff:0:0:0:0stillnoblank'
cmp_exact
exit_test
