#!/bin/bash
# added 2018-04-27 by alorbach
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1000
generate_conf
add_conf '
global(	defaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'"
	defaultNetstreamDriverCertFile="'$srcdir/tls-certs/cert.pem'"
	defaultNetstreamDriverKeyFile="'$srcdir/tls-certs/key.pem'"
)
module(	load="../plugins/imtcp/.libs/imtcp"
	StreamDriver.Name="ossl"
	StreamDriver.TlsVerifyDepth="5" 
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/certvalid" )
input(	type="imtcp" port="'$TCPFLOOD_PORT'" )

$template outfmt,"%msg:F,58:2%\n"
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

# Begin actual testcase
startup
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
wait_file_lines
shutdown_when_empty
wait_shutdown
seq_check
exit_test
