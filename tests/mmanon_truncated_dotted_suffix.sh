#!/bin/bash
# This test hardens mmanon against truncated dotted IPv4-looking suffixes.
# The oracle is normal shutdown plus exact output: malformed suffixes at the
# end of a rewritten message must be preserved and must not read past the
# message buffer while IPv4 and embedded-IPv4 scanners continue.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv4.mode="zero" ipv4.bits="32" ipv6.enable="off" embeddedipv4.anonmode="zero" embeddedipv4.bits="128")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: 1.2.3.
<129>Mar 10 01:00:00 172.20.245.8 tag: 1.2.
<129>Mar 10 01:00:00 172.20.245.8 tag: 1.
<129>Mar 10 01:00:00 172.20.245.8 tag: 10.20.30.40 1.2.3.
<129>Mar 10 01:00:00 172.20.245.8 tag: aa:bb::1.2.3.\""

shutdown_when_empty
wait_shutdown
export EXPECTED=' 1.2.3.
 1.2.
 1.
 0.0.0.0 1.2.3.
 aa:bb::1.2.3.'
cmp_exact
exit_test
