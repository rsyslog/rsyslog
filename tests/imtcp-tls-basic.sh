#!/bin/bash
# added 2011-02-28 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp

$DefaultNetstreamDriver gtls

$DefaultNetstreamDriverCAFile '$srcdir'/tls-certs/ca.pem
$DefaultNetstreamDriverCertFile '$srcdir'/tls-certs/cert.pem
$DefaultNetstreamDriverKeyFile '$srcdir'/tls-certs/key.pem
$InputTCPServerStreamDriverMode 1
$InputTCPServerStreamDriverAuthMode anon
$InputTCPServerRun '$TCPFLOOD_PORT'

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
seq_check
exit_test
