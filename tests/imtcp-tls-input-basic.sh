#!/bin/bash
# added 2011-02-28 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
        streamDriver.CAFile="'$srcdir'/tls-certs/ca.pem"
	streamDriver.CertFile="'$srcdir'/tls-certs/cert.pem"
	streamDriver.KeyFile="'$srcdir'/tls-certs/key.pem")


template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
seq_check
exit_test
