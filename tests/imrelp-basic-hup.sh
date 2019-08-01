#!/bin/bash
# added 2019-07-30 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=4000 # MUST be an even number!
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$TCPFLOOD_PORT'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
tcpflood -Trelp-plain -p$TCPFLOOD_PORT -m$((NUMMESSAGES / 2))
issue_HUP
tcpflood -Trelp-plain -p$TCPFLOOD_PORT -m$((NUMMESSAGES / 2)) -i$((NUMMESSAGES / 2))
shutdown_when_empty
wait_shutdown
seq_check
exit_test
