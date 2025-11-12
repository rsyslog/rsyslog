#!/bin/bash
# This test checks imtcp functionality with multiple drivers running together. It is
# a minimal test.
# added 2021-04-27 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=60000 # must be evenly dividable by 3
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem")

module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	name="i1"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )

input(type="imtcp" name="i2" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port3"
	name="i3"
	StreamDriver.Name="mbedtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
assign_rs_port "$RSYSLOG_DYNNAME.tcpflood_port3"
tcpflood -p$TCPFLOOD_PORT -m$((NUMMESSAGES / 3)) -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
tcpflood -p$TCPFLOOD_PORT2 -m$((NUMMESSAGES / 3)) -i$((NUMMESSAGES / 3))
tcpflood -p$RS_PORT -m$((NUMMESSAGES / 3)) -i$((NUMMESSAGES / 3 * 2)) -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
seq_check
exit_test
