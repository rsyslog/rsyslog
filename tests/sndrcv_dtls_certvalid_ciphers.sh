#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
# start up the instances
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
export NUMMESSAGES=1
generate_conf
export PORT_RCVR="$(get_free_port)"

add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
#	debug.whitelist="on"
#	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imdtls/.libs/imdtls" 
	tls.AuthMode="anon")

input(	type="imdtls"
	tls.tlscfgcmd="CipherString=ECDHE-RSA-AES256-SHA384;Ciphersuites=TLS_AES_256_GCM_SHA384"
	port="'$PORT_RCVR'")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.sender.debuglog"
generate_conf 2
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)

module(	load="../plugins/omdtls/.libs/omdtls" )

action(	name="omdtls"
	type="omdtls"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	tls.tlscfgcmd="CipherString=ECDHE-RSA-AES128-GCM-SHA256\nCiphersuites=TLS_AES_128_GCM_SHA256"
)
' 2
startup 2

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
injectmsg 0 $NUMMESSAGES

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
