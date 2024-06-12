#!/bin/bash
# test for b64_decode function in rainerscript
# added 2024-06-11 by KGuillemot
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

set $!str!var1 = b64_decode("");          # Empty string
set $!str!var2 = b64_decode("AAAAA");     # Invalid base64
set $!str!var3 = b64_decode("dGVzdA==");  # "test" base64
set $!str!var4 = b64_decode("dGVzdA");    # "test" base64 - without padding
set $!str!var5 = b64_decode("AQI");       # \x01\x02 base64 (binary)
set $!str!var6 = b64_decode("AQI=");      # \x01\x02 base64 (binary) - with padding
set $!str!var7 = b64_decode("dGVzdA==dGVzdA==");  # Early ended payload
set $!str!var8 = b64_decode("YWJjZAplZmdoCg==");  # \n in encoded data
set $!str!var9 = b64_decode("YWJjZA1lZmdoCg==");  # \r in encoded data

template(name="outfmt" type="string" string="%!str%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown
# var1 is not present (empty)
# var2 is empty (invalid base64)
if ! cmp $RSYSLOG_OUT_LOG '{ "var2": "", "var3": "test", "var4": "test", "var5": "\u0001\u0002", "var6": "\u0001\u0002", "var7": "test", "var8": "abcd\nefgh\n", "var9": "abcd\refgh\n" }' ; then
  echo "invalid function output detected, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;
exit_test

