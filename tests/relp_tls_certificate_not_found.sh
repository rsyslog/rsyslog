#!/bin/bash
# add 2017-09-21 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '

module(load="../plugins/omrelp/.libs/omrelp")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="ruleset")
input(type="imtcp" port="13514")

ruleset(name="ruleset") {
	action(type="omrelp" target="127.0.0.1" port="10514" tls="on" tls.authMode="name" tls.caCert="tls-certs/ca.pem" tls.myCert="tls-certs/fake-cert.pem" tls.myPrivKey="tls-certs/fake-key.pem" tls.permittedPeer=["rsyslog-test-root-ca"])
}

action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "certificate file tls-certs/fake-cert.pem.*No such file" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from missing input file not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
