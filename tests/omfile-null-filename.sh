#!/bin/bash
# check that omfile does not segfault when filename is given but empty.
# addd 2018-04-03 by RGerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
action(type="omfile" file="")
action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "parameter must be given" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	. $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
