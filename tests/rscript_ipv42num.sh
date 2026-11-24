#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# add 2017-02-09 by Jan Gerhards, released under ASL 2.0
# Check valid IPv4 octets and reject 256 in every position. The output
# comparison proves invalid addresses return -1 instead of colliding with
# a valid address's numeric value.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# in pre 8.1907.0 versions of rsyslog the code was misspelled as
# "ip42num" (missing "v"). We check this is still supported as alias
set $!ip!v0 = ip42num("0.0.0.0");

# use correct function name
set $!ip!v1 = ipv42num("0.0.0.0");
set $!ip!v2 = ipv42num("0.0.0.1");
set $!ip!v3 = ipv42num("0.0.1.0");
set $!ip!v4 = ipv42num("0.1.0.0");
set $!ip!v5 = ipv42num("1.0.0.0");
set $!ip!v6 = ipv42num("0.0.0.135");
set $!ip!v7 = ipv42num("1.1.1.1");
set $!ip!v8 = ipv42num("225.33.1.10");
set $!ip!v9 = ipv42num("172.0.0.1");
set $!ip!v10 = ipv42num("255.255.255.255");
set $!ip!v11 = ipv42num("1.0.3.45         ");
set $!ip!v12 = ipv42num("      0.0.0.1");
set $!ip!v13 = ipv42num("    0.0.0.1   ");
set $!ip!v14 = ipv42num("000.000.000.255");

set $!ip!e1 = ipv42num("a");
set $!ip!e2 = ipv42num("");
set $!ip!e3 = ipv42num("123.4.6.*");
set $!ip!e4 = ipv42num("172.0.0.1.");
set $!ip!e5 = ipv42num("172.0.0..1");
set $!ip!e6 = ipv42num(".172.0.0.1");
set $!ip!e7 = ipv42num(".17 2.0.0.1");
set $!ip!e8 = ipv42num("256.0.0.0");
set $!ip!e9 = ipv42num("0.256.0.0");
set $!ip!e10 = ipv42num("0.0.256.0");
set $!ip!e11 = ipv42num("0.0.0.256");
set $!ip!e12 = ipv42num("999999999999999999999.0.0.0");


template(name="outfmt" type="string" string="%!ip%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown
echo '{ "v0": 0, "v1": 0, "v2": 1, "v3": 256, "v4": 65536, "v5": 16777216, "v6": 135, "v7": 16843009, "v8": 3777036554, "v9": 2885681153, "v10": 4294967295, "v11": 16778029, "v12": 1, "v13": 1, "v14": 255, "e1": -1, "e2": -1, "e3": -1, "e4": -1, "e5": -1, "e6": -1, "e7": -1, "e8": -1, "e9": -1, "e10": -1, "e11": -1, "e12": -1 }' | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid function output detected, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;
exit_test

