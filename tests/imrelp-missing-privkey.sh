#!/bin/bash
# addd 2018-06-05 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(
    type="imrelp"
    tls="on"
    port="1604"
    tls.myCert="nocert"
)

action(type="omfile" file="rsyslog.out.log")
'
touch nocert # it is not important that this is a real cert, it just must exist!
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
EXPECTED="no .* private key file"
. $srcdir/diag.sh grep-check
. $srcdir/diag.sh exit
