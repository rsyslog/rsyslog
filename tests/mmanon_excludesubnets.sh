#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Verify that mmanon's opt-in excludeSubnets preserves only matching address
# literals while still anonymizing nonmatches in the same message. The output
# files are the oracle for the empty default, IPv4/IPv6 subnet boundaries,
# embedded IPv4-in-IPv6 OR matching, and skipping a preserved literal before
# the IPv4 and IPv6 scanners can rewrite part of it.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
    if ($msg contains "empty-default") then {
        action(type="mmanon" ipv4.bits="32" ipv6.bits="128")
        action(type="omfile" file="'${RSYSLOG_DYNNAME}'.default.log" template="outfmt")
    } else {
        action(type="mmanon" ipv4.bits="32" ipv6.bits="128"
               embeddedIPv4.bits="128" embeddedIPv4.anonMode="zero"
               excludeSubnets=["192.0.2.0/24", "2001:db8:2::/48"])
        action(type="omfile" file="'${RSYSLOG_DYNNAME}'.excluded.log" template="outfmt")
    }
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: empty-default 192.0.2.1
<129>Mar 10 01:00:00 172.20.245.8 tag: IPv4-boundary 192.0.2.0 192.0.2.255 outside 192.0.3.0
<129>Mar 10 01:00:00 172.20.245.8 tag: IPv6-boundary 2001:db8:2:: 2001:db8:2:ffff:ffff:ffff:ffff:ffff outside 2001:db8:3::1
<129>Mar 10 01:00:00 172.20.245.8 tag: embedded-IPv4-match 2001:db8:9::192.0.2.1
<129>Mar 10 01:00:00 172.20.245.8 tag: embedded-IPv6-match 2001:db8:2::192.0.3.1
<129>Mar 10 01:00:00 172.20.245.8 tag: embedded-no-match 2001:db8:3::192.0.3.1\""

shutdown_when_empty
wait_shutdown

export EXPECTED=' empty-default 0.0.0.0'
cmp_exact "${RSYSLOG_DYNNAME}.default.log"

export EXPECTED=' IPv4-boundary 192.0.2.0 192.0.2.255 outside 0.0.0.0
 IPv6-boundary 2001:db8:2:: 2001:db8:2:ffff:ffff:ffff:ffff:ffff outside 0:0:0:0:0:0:0:0
 embedded-IPv4-match 2001:db8:9::192.0.2.1
 embedded-IPv6-match 2001:db8:2::192.0.3.1
 embedded-no-match 0:0:0:0:0:0:0.0.0.0'
cmp_exact "${RSYSLOG_DYNNAME}.excluded.log"
exit_test
