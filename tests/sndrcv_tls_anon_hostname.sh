#!/bin/bash
# rgerhards, 2011-04-04
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=25000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
# start up the instances
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
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
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" )

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
export RCVR_PORT=$TCPFLOOD_PORT
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
global(
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="gtls"
)

# set up the action
$ActionSendStreamDriverMode 1 # require TLS for the connection
$ActionSendStreamDriverAuthMode anon
*.*	@@localhost:'$RCVR_PORT'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check
exit_test
