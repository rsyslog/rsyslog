#!/bin/bash
# added 2021-08-03 by Rgerhards
# check that we do use the proper set of certificates
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
# The default shall NOT be used - if so, tcpflood would err out!

global( defaultNetstreamDriverCAFile="'$srcdir'/testsuites/x.509/ca.pem"
	defaultNetstreamDriverCertFile="'$srcdir'/testsuites/x.509/machine-cert.pem"
	defaultNetstreamDriverKeyFile="'$srcdir'/testsuites/x.509/machine-key.pem")

module(load="../plugins/imtcp/.libs/imtcp"
	permittedPeer="SHA1:5C:C6:62:D5:9D:25:9F:BC:F3:CB:61:FA:D2:B3:8B:61:88:D7:06:C3"
	StreamDriver.Name="ossl"
	StreamDriver.Mode="1"
	StreamDriver.AuthMode="x509/fingerprint" )

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
        streamDriver.CAFile="'$srcdir'/tls-certs/ca.pem"
	streamDriver.CertFile="'$srcdir'/tls-certs/cert.pem"
	streamDriver.KeyFile="'$srcdir'/tls-certs/key.pem")


template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
seq_check
exit_test
