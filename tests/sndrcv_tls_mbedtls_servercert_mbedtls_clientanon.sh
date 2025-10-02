#!/bin/bash
# alorbach, 2019-01-16
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="mbedtls"
	StreamDriver.Mode="1" StreamDriver.AuthMode="anon" )

# then SENDER sends to this port (not tcpflood!)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
export RCVR_PORT=$TCPFLOOD_PORT
#export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
global(defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'")

action(	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$RCVR_PORT'"
	StreamDriver="mbedtls"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/certvalid"
	)
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2
wait_file_lines
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check
exit_test
