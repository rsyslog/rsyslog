#!/bin/bash
# added 2018-04-27 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.receiver.debuglog"
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
#	debug.whitelist="on"
#	debug.files=["nsd_ossl.c", "tcpsrv.c", "nsdsel_ossl.c", "nsdpoll_ptcp.c", "dnscache.c"]
)

module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="anon"
	gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.2
	MinProtocol=TLSv1.1
	Options=Bugs"
	)
input(	type="imtcp"
	port="'$TCPFLOOD_PORT'" )

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

# now inject the messages which will fail due protocol configuration
tcpflood --check-only -k "Protocol=ALL,-SSLv2,-SSLv3,-TLSv1,-TLSv1.1,-TLSv1.3" -p'$TCPFLOOD_PORT' -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem

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