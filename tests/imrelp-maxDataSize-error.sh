#!/bin/bash
# add 2018-04-26 by Pascal Withopf, released under ASL 2.0
echo [imrelp-maxDataSize-error.sh]
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imrelp/.libs/imrelp")

global(
	maxMessageSize="300"
)

input(type="imrelp" port="13514" maxDataSize="250")

action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
./msleep 2000

. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "error: maxDataSize is smaller than global parameter maxMessageSize" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message not found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
