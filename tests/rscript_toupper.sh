#!/bin/bash
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

set $!str!var1 = toupper("test");
set $!str!var2 = toupper("TeSt");
set $!str!var3 = toupper("");

template(name="outfmt" type="string" string="%!str%\n")
local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
tcpflood -m1 -y
shutdown_when_empty
wait_shutdown
export EXPECTED='{ "var1": "TEST", "var2": "TEST", "var3": "" }'
cmp_exact
exit_test

