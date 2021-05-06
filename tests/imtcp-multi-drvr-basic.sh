#!/bin/bash
# This test checks imtcp functionality with multiple drivers running together. It is
# a minimal test.
# added 2021-04-27 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000 # must be even number!
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem")

module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
tcpflood -p$TCPFLOOD_PORT -m$((NUMMESSAGES / 2)) -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
tcpflood -p$TCPFLOOD_PORT2 -m$((NUMMESSAGES / 2)) -i$((NUMMESSAGES / 2))
shutdown_when_empty
wait_shutdown
seq_check
exit_test
