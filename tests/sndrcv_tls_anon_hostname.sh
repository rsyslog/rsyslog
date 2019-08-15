#!/bin/bash
# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=25000
# start up the instances
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
global(	
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="gtls"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )
# then SENDER sends to this port (not tcpflood!)
input(	type="imtcp" port="'$PORT_RCVR'" )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)" # TODO: move to diag.sh
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

# Note: no TLS for the listener, this is for tcpflood!
$ModLoad ../plugins/imtcp/.libs/imtcp
input(	type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

# set up the action
$ActionSendStreamDriverMode 1 # require TLS for the connection
$ActionSendStreamDriverAuthMode anon
*.*	@@localhost:'$PORT_RCVR'
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
