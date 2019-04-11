#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# uncomment for debugging support:
. ${srcdir:=.}/diag.sh init
# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
export PORT_RCVR="$(get_free_port)"
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="ossl"
#	debug.whitelist="on"
#	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid"
	StreamDriver.PermitExpiredCerts="off"
	gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2
	MinProtocol=TLSv1.1
	Options=Bugs"
	)
input(	type="imtcp"
	port="'$PORT_RCVR'" )

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
#valgrind="valgrind"
generate_conf 2
export TCPFLOOD_PORT="$(get_free_port)" # TODO: move to diag.sh
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
	defaultNetstreamDriver="ossl"
)

# Note: no TLS for the listener, this is for tcpflood!
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun '$TCPFLOOD_PORT'

# set up the action
action(	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriverMode="1"
	StreamDriverAuthMode="x509/certvalid"
	gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.1,-TLSv1.3"
)
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m1 -i1
# sleep 5 # make sure all data is received in input buffers
# shut down sender when everything is sent, receiver continues to run concurrently
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

content_check --check-only "OpenSSL Version to old"
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: OpenSSL Version to old"
	skip_test
else
	content_check "wrong version number"
	content_check "OpenSSL Error Stack:"
fi



unset PORT_RCVR # TODO: move to exit_test()?
exit_test