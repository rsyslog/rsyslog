#!/bin/bash
# added 2019-11-13 by alorbach
# Verify RELP TLS configuration command handling for incompatible receiver and
# sender protocol constraints. The receiver binds an ephemeral IPv4 listener and
# the testbench discovers that bound port after startup, avoiding a
# preselected-port race before the sender connects. Success is the expected
# librelp TLS failure on the sender side, verified by the sender debug log after
# both instances shut down. Different librelp/OpenSSL combinations may report
# different RELP error codes for the same failed TLS negotiation, so the oracle
# checks the generic librelp ecode prefix. Compatibility skips handle
# unsupported TLS libraries or OpenSSL versions.
. ${srcdir:=.}/diag.sh init
require_relpEngineSetTLSLibByName
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
module(	load="../plugins/imrelp/.libs/imrelp" 
	tls.tlslib="openssl")
# then SENDER sends to this port (not tcpflood!)
input(	type="imrelp" address="127.0.0.1" port="0" tls="on"
	tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2
CipherString=ECDHE-RSA-AES256-GCM-SHA384
Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2,-TLSv1.3
MinProtocol=TLSv1.2
MaxProtocol=TLSv1.2" 
	)

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
assign_single_tcp_listener_port PORT_RCVR

export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
module(	load="../plugins/omrelp/.libs/omrelp" 
	tls.tlslib="openssl")

action(	type="omrelp" target="127.0.0.1" port="'$PORT_RCVR'" tls="on"
	tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1.1,-TLSv1.2
CipherString=DHE-RSA-AES256-SHA
Protocol=ALL,-SSLv2,-SSLv3,-TLSv1.1,-TLSv1.2,-TLSv1.3
MinProtocol=TLSv1.1
MaxProtocol=TLSv1.1" )
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg2 1 1000

# shut down sender
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

content_check --check-only "relpTcpConnectTLSInit_gnutls" ${RSYSLOG_DEBUGLOG}
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: LIBRELP was build without OPENSSL Support"
	skip_test
fi 

content_check --check-only "OpenSSL Version too old" $RSYSLOG_DEBUGLOG
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: OpenSSL Version too old"
	skip_test
else
	# Kindly check for a failed session
	content_check "librelp: generic error: ecode" $RSYSLOG_DEBUGLOG
#	content_check "OpenSSL Error Stack:"
fi

exit_test
