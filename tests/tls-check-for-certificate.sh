#!/bin/bash
# add 2017-05-05 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(
	defaultNetstreamDriver="gtls"
	defaultNetstreamDriverKeyFile="tls-certs/key.pem"
	defaultNetstreamDriverCertFile="tls-certs/cert.pem"
	defaultNetstreamDriverCaFile="tls-certs/ca.pem"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="gtls" StreamDriver.Mode="1" StreamDriver.AuthMode="anon")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum" then {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
tcpflood -m1000 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
seq_check 0 999

exit_test
