#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global(processInternalMessages="on")
module(load="../plugins/imtcp/.libs/imtcp" maxFrameSize="100")
input(type="imtcp" port="13514")

action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"1005 <120> 2011-03-01T11:22:12Z host tag: this is a way too long message\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "Framing Error.*change to octet stuffing" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from imtcp not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi


. $srcdir/diag.sh exit
