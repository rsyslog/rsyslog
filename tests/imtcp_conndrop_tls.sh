#!/bin/bash
# Test imtcp/TLS with many dropping connections
# added 2011-06-09 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
global(	maxMessageSize="10k"
	defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem"
	defaultNetstreamDriver="gtls"
	debug.whitelist="on"
	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(load="../plugins/imtcp/.libs/imtcp" maxSessions="1100"
       streamDriver.mode="1" streamDriver.authMode="anon")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
local0.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
# 100 byte messages to gain more practical data use
tcpflood -c20 -p$TCPFLOOD_PORT -m$NUMMESSAGES -r -d100 -P129 -D -l0.995 -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
