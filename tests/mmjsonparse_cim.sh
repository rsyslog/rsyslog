#!/bin/bash
# added 2014-07-15 by rgerhards
# basic test for mmjsonparse module with "cim" cookie
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!cim!msgnum%\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="mmjsonparse" cookie="@cim:" container="!cim")
if $parsesuccess == "OK" then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m $NUMMESSAGES -j "@cim: "
shutdown_when_empty
wait_shutdown 
seq_check
exit_test
