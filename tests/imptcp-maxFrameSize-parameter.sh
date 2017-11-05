#!/bin/bash
# addd 2017-03-01 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$MaxMessageSize 12800
global(processInternalMessages="on")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" maxframesize="100")

action(type="omfile" file="rsyslog.out.log")

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"10005 <120> 2011-03-01T11:22:12Z host tag: this is a way too long message\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "Framing Error.*change to octet stuffing" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from imptcp truncation not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
