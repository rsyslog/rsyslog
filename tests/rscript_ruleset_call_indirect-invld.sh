#!/bin/bash
# added 2016-12-11 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="list") {
	property(name="msg" field.delimiter="58" field.number="2")
	constant(value="\n")
}

ruleset(name="rs") {
	action(type="omfile" file="./rsyslog2.out.log" template="outfmt")
}

if $msg contains "msgnum" then
	call_indirect "does-not-exist";
else
	action(type="omfile" file="./rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 5
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
grep "error.*does-not-exist" rsyslog.out.log > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	. $srcdir/diag.sh error-exit 1
fi
. $srcdir/diag.sh exit
