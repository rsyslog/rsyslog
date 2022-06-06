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
	action(type="mmanon" ipv6.anonmode="zero")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: asdfghjk
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:c820:1180:c84c:ad3f:4024:d991:ec2e:4922
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:c820:1180:c84c:ad3f:4024:d991:ec2e
<129>Mar 10 01:00:00 172.20.245.8 tag: [1a00:c820:1180:c84c:ad3f:4024:d991:ec2e]:4922
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:c820:1180:c84c:ad3f::d991:ec2e:4922
<129>Mar 10 01:00:00 172.20.245.8 tag: [1a00:c820:1180:c84c:ad3f::d991:ec2e]:4922
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:c820:1180:c84c:ad3f::d991:ec2e:49225
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:4922:4922:c84c:ad3f::d991:ec2e:49225
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:4922:1180:c84c:ad3f::d991:4922:49225
<129>Mar 10 01:00:00 172.20.245.8 tag: 1a00:c820:49225:c84c:ad3f::d991:ec2e:49225\""

# see  this github comment on limits of IP address detection:
# https://github.com/rsyslog/rsyslog/issues/4856#issuecomment-1108445473

shutdown_when_empty
wait_shutdown
export EXPECTED=' asdfghjk
 1a00:c820:0:0:0:0:0:0:4922
 1a00:c820:0:0:0:0:0:0
 [1a00:c820:0:0:0:0:0:0]:4922
 1a00:c820:1180:0:0:0:0:0:0
 [1a00:c820:0:0:0:0:0:0]:4922
 1a00:c820:0:0:0:0:0:0:49225
 1a00:4922:0:0:0:0:0:0:49225
 1a00:4922:0:0:0:0:0:0:49225
 1a00:c820:49225:c84c:0:0:0:0:0:0:49225'
cmp_exact
exit_test
