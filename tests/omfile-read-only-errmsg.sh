#!/bin/bash
# addd 2017-03-01 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfile" template="outfmt" file="rsyslog2.out.log")
}

action(type="omfile" file="rsyslog.out.log")
'
touch rsyslog2.out.log
chmod 0400 rsyslog2.out.log
ls -l rsyslog.ou*
. $srcdir/diag.sh startup
$srcdir/diag.sh injectmsg 0 1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "rsyslog2.out.log.* open error" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	. $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
