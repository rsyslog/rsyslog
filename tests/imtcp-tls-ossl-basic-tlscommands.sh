#!/bin/bash
# added 2018-04-27 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
# uncomment for debugging support:
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
#	debug.whitelist="on"
#	debug.files=["net_ossl.c", "nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2
	Options=Bugs"
	)
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

# now inject the messages which will fail due protocol configuration
tcpflood --check-only -k "Protocol=-ALL,TLSv1.2" -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem

shutdown_when_empty
wait_shutdown

if content_check --check-only "TLS library does not support SSL_CONF_cmd"
then
	echo "SKIP: TLS library does not support SSL_CONF_cmd"
	skip_test
else
	if content_check --check-only "SSL_ERROR_SYSCALL"
	then
		# Found SSL_ERROR_SYSCALL errorcode, no further check needed
		exit_test
	else
		# Check for a SSL_ERROR_SSL error code
		content_check "SSL_ERROR_SSL"
		content_check "OpenSSL Error Stack:"
	fi
fi

exit_test
