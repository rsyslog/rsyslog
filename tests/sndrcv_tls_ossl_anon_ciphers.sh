#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# start up the instances
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(	
	defaultNetstreamDriver="ossl"
	debug.whitelist="on"
	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	StreamDriver.PermitExpiredCerts="off"
	gnutlsPriorityString="CipherString=ECDHE-RSA-AES256-SHA384;Ciphersuites=TLS_AES_256_GCM_SHA384"
	)
input(	type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
export PORT_RCVR=$TCPFLOOD_PORT # save this, will be rewritten with next config
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
global(	
	defaultNetstreamDriver="ossl"
)

action(	type="omfwd"
	protocol="tcp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	StreamDriverMode="1"
	StreamDriverAuthMode="anon"
	gnutlsPriorityString="CipherString=ECDHE-RSA-AES256-SHA384;Ciphersuites=TLS_AES_128_GCM_SHA256"
)
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg 0 1
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

# IMPORTANT: this test will generate many error messages. This is exactly it's
# intent. So do not think something is wrong. The content_check below checks
# these error codes.

content_check --check-only "TLS library does not support SSL_CONF_cmd"
ret=$?
if [ $ret == 0 ]; then
	echo "SKIP: TLS library does not support SSL_CONF_cmd"
	skip_test
else
	# Kindly check for a failed session
	content_check "OpenSSL Error Stack"
	content_check "no shared cipher"
fi

exit_test
