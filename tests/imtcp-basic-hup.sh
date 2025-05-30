#!/bin/bash
# added 2019-07-30 by RGerhards, released under ASL 2.0
export NUMMESSAGES=4000 # MUST be an even number!
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -p$TCPFLOOD_PORT -m$((NUMMESSAGES / 2))
issue_HUP
tcpflood -p$TCPFLOOD_PORT -m$((NUMMESSAGES / 2)) -i$((NUMMESSAGES / 2))
shutdown_when_empty
wait_shutdown
seq_check
exit_test
