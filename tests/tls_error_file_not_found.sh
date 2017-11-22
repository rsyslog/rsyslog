#!/bin/bash
# add 2017-09-21 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(
	defaultNetstreamDriver="gtls"
	defaultNetstreamDriverKeyFile="tls-certs/nokey.pem"
	defaultNetstreamDriverCertFile="tls-certs/cert.pem"
	defaultNetstreamDriverCaFile="tls-certs/ca.pem"
)
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="gtls" StreamDriver.Mode="1" StreamDriver.AuthMode="anon")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

action(type="omfile" template="outfmt" file="rsyslog.out.log")

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "defaultnetstreamdriverkeyfile.*tls-certs/nokey.pem" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from missing input file not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
