#!/bin/bash
# Added 2025-12-19 by 20syldev, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!result%\n")

if $msg contains "msgnum:" then {
    set $!input = "abc@example.com, def@example.com, ghi@example.com";
    set $!result = split($!input, ", ");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

export EXPECTED='[ "abc@example.com", "def@example.com", "ghi@example.com" ]'
cmp_exact $RSYSLOG_OUT_LOG
exit_test
