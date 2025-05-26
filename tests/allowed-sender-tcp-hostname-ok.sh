#!/bin/bash
# check that we are able to receive messages from allowed sender
# added 2019-08-15 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
setvar_RS_HOSTNAME
export NUMMESSAGES=5 # it's just important that we get any messages at all
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$AllowedSender TCP,*'$RS_HOSTNAME'*
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
