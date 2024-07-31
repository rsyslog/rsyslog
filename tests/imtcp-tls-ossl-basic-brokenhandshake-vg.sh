#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
if [ "$(valgrind --version)" == "valgrind-3.11.0" ]; then
	printf 'This test does NOT work with valgrind-3.11.0 - valgrind always reports\n'
	printf 'a valgrind-internal bug. So we need to skip it.\n'
	exit 77
fi
. ${srcdir:=.}/diag.sh init
export USE_VALGRIND="YES"
export NUMMESSAGES=1
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
	StreamDriver.AuthMode="anon" )
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(	type="omfile" 
					template="outfmt"
					file=`echo $RSYSLOG_OUT_LOG`)
'
# Begin actual testcase | send one msg without TLS to force a handshake failure, send second msg with TLS to make the test PASS
startup
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES 
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem

wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
