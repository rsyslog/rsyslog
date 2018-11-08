#!/bin/bash
# rgerhards, 2011-04-04
# testing sending and receiving via TLS with anon auth and rebind
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# debugging activated to try to solve https://github.com/rsyslog/rsyslog/issues/3256
export RSYSLOG_DEBUG="debug nostdout"
test_error_exit_handler() {
	set -x
	cat "$RSYSLOG_DYNNAME.receiver.debuglog"
	cat "$RSYSLOG_DYNNAME.sender.debuglog"
	set +x
}

#receiver
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
global(	
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="ossl"
	debug.whitelist="on"
	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon" )
input(	type="imtcp"
	port="'$PORT_RCVR'" )

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(	type="omfile" 
					template="outfmt"
					file="'$RSYSLOG_OUT_LOG'")
'
startup

#sender
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)" # TODO: move to diag.sh
add_conf '
global(	
	defaultNetstreamDriverCAFile="'$srcdir/testsuites/x.509/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/testsuites/x.509/client-cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/testsuites/x.509/client-key.pem'"
	defaultNetstreamDriver="ossl"
	debug.whitelist="on"
	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)


# Note: no TLS for the listener, this is for tcpflood!
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun '$TCPFLOOD_PORT'

# set up the action
$DefaultNetstreamDriver ossl # use gtls netstream driver
$ActionSendStreamDriverMode 1 # require TLS for the connection
$ActionSendStreamDriverAuthMode anon
$ActionSendTCPRebindInterval 100
*.*	@@127.0.0.1:'$PORT_RCVR'
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m25000 -i1
sleep 5 # make sure all data is received in input buffers
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

seq_check 1 25000 -d

unset PORT_RCVR
exit_test
