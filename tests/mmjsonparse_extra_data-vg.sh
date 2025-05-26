#!/bin/bash
# ensure mmjsonparse works correctly if passed unparsable message
# add 2018-11-05 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="-%msg%-\n")

action(type="mmjsonparse" cookie="")
if $parsesuccess == "OK" then {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
else if $parsesuccess == "FAIL" and $msg contains "param1" then {
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="outfmt")
}
'
startup_vg
tcpflood -m2 -M "\"{\\\"param1\\\":\\\"value1\\\"} data\""
shutdown_when_empty
wait_shutdown_vg
check_exit_vg

check_file_not_exists $RSYSLOG_OUT_LOG
export EXPECTED='-{"param1":"value1"} data-
-{"param1":"value1"} data-'
cmp_exact $RSYSLOG2_OUT_LOG
exit_test
