#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)


# NOTE: we intentionally use legacy statements here! This *IS* what we want to test!
$ModLoad ../plugins/imtcp/.libs/imtcp
$DefaultNetstreamDriver gtls
$inputTcpserverStreamdriverPermittedPeer rsyslog-client

$InputTCPServerStreamDriverAuthMode x509/name
$InputTCPServerStreamDriverPermittedPeer Log_Streaming_Client
$InputTCPServerStreamDriverMode 1
$InputTCPServerListenPortFile '$RSYSLOG_DYNNAME'.tcpflood_port
$InputTCPServerRun 0

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(	type="omfile" 
					template="outfmt"
					file=`echo $RSYSLOG_OUT_LOG`)
'
startup
tcpflood -p'$TCPFLOOD_PORT' -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
