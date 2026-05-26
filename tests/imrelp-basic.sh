#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0
#
# Verify basic plain RELP input delivery from tcpflood through imrelp. The
# receiver binds an ephemeral TCP port and the testbench discovers the actual
# listener from the running rsyslog process, avoiding get_free_port races with
# parallel tests. Success is the exact sequence check for all injected messages.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10000 # MUST be an even number!
generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" address="127.0.0.1" port="0")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG_OUT_LOG`)
'
startup
assign_single_tcp_listener_port TCPFLOOD_PORT
tcpflood -Trelp-plain -p$TCPFLOOD_PORT -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
