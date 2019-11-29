#!/bin/bash
# a case that actually caused a segfault
# add 2017-11-06 by PascalWithopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmexternal/.libs/mmexternal")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
set $!x = "a";

template(name="outfmt" type="string" string="-%$!%-\n")

if $msg contains "msgnum:" then {
	action(type="mmexternal" interface.input="fulljson"
		binary="'$PYTHON' '${srcdir}'/testsuites/mmexternal-SegFault-mm-python.py")
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup
#startup_vg
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag:msgnum:1\""
shutdown_when_empty
wait_shutdown
#wait_shutdown_vg
#check_exit_vg

export EXPECTED='-{ "x": "a", "sometag": "somevalue" }-'
cmp_exact
exit_test
