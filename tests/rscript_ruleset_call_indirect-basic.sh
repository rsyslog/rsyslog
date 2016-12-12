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
	action(type="omfile" file="./rsyslog.out.log" template="outfmt")
}

if $msg contains "msgnum" then call_indirect "r" & "s";
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 100
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh seq-check  0 99
. $srcdir/diag.sh exit
