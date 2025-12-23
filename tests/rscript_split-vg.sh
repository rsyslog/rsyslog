#!/bin/bash
# Added 2025-12-19 by 20syldev, released under ASL 2.0
export USE_VALGRIND="YES"
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$!result%\n")

if $msg contains "msgnum:" then {
    set $!input = "slambert@zenetys.com, bdolez@zenetys.com, contact@sylvain.pro";
    set $!result = split($!input, ", ");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup_vg
tcpflood -m1
shutdown_when_empty
wait_shutdown_vg
check_exit_vg

export EXPECTED='[ "slambert@zenetys.com", "bdolez@zenetys.com", "contact@sylvain.pro" ]'
cmp_exact $RSYSLOG_OUT_LOG
exit_test
