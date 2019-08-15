#!/bin/bash
# all we want to test is if certless communication works. So we do
# not need to send many messages.
# This test checks legacy statements which are often given as 
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
# receiver
export PORT_RCVR="$(get_free_port)"
add_conf '
global( defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="gtls"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )
# then SENDER sends to this port (not tcpflood!)
input(	type="imtcp" port="'$PORT_RCVR'" )

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup

# sender
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
#export TCPFLOOD_PORT="$(get_free_port)"
add_conf '
# Note: no TLS for the listener, this is for tcpflood!
$ModLoad ../plugins/imtcp/.libs/imtcp
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

# NOTE: do NOT change legacy statements - they are used intentionally
$DefaultNetstreamDriverCAFile '$srcdir/tls-certs/ca.pem'
$ActionSendStreamDriver gtls
$ActionSendStreamDriverMode 1
$ActionSendStreamDriverAuthMode x509/name
$ActionSendStreamDriverPermittedPeer *.rsyslog.com
*.* @@localhost:'$PORT_RCVR'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m$NUMMESSAGES -i1
wait_file_lines
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check 1 $NUMMESSAGES
exit_test
