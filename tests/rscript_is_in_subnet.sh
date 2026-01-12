#!/bin/bash
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# Basic IPv4 tests
set $!res!v4_1 = is_in_subnet("192.168.1.5", "192.168.1.0/24");
set $!res!v4_2 = is_in_subnet("192.168.2.5", "192.168.1.0/24");
set $!res!v4_3 = is_in_subnet("192.168.1.1", "192.168.1.1/32");
set $!res!v4_4 = is_in_subnet("192.168.1.1", "0.0.0.0/0");

# Basic IPv6 tests
set $!res!v6_1 = is_in_subnet("2001:db8::1", "2001:db8::/32");
set $!res!v6_2 = is_in_subnet("2001:db9::1", "2001:db8::/32");
set $!res!v6_3 = is_in_subnet("::1", "::1/128");
set $!res!v6_4 = is_in_subnet("::1", "::/0");

# Mixed / Invalid tests
set $!res!inv_1 = is_in_subnet("192.168.1.1", "2001:db8::/32");
set $!res!inv_2 = is_in_subnet("invalid", "192.168.1.0/24");
set $!res!inv_3 = is_in_subnet("192.168.1.1", "invalid");
set $!res!inv_4 = is_in_subnet("192.168.1.1", "192.168.1.0/33"); # Invalid mask

template(name="outfmt" type="string" string="%!res%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown

export EXPECTED='{ "v4_1": 1, "v4_2": 0, "v4_3": 1, "v4_4": 1, "v6_1": 1, "v6_2": 0, "v6_3": 1, "v6_4": 1, "inv_1": 0, "inv_2": 0, "inv_3": 0, "inv_4": 0 }'
cmp_exact
exit_test
