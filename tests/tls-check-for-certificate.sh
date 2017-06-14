#!/bin/bash
# add 2017-05-05 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(
	defaultNetstreamDriver="gtls"
	defaultNetstreamDriverKeyFile="tls-certs/key.pem"
	defaultNetstreamDriverCertFile="tls-certs/cert.pem"
	defaultNetstreamDriverCaFile="tls-certs/ca.pem"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="gtls" StreamDriver.Mode="1" StreamDriver.AuthMode="anon")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum" then {
	action(type="omfile" template="outfmt" file="rsyslog.out.log")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1000 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 999

. $srcdir/diag.sh exit
