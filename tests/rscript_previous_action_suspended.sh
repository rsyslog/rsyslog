#!/bin/bash
# Added 2017-12-09 by Rainer Gerhards, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omtesting/.libs/omtesting")
input(type="imtcp" port="13514")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

ruleset(name="output_writer") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}

:msg, contains, "msgnum:" {
	:omtesting:fail 2 0
	if previous_action_suspended() then
		call output_writer
}
'

. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg 0 10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 1 9
. $srcdir/diag.sh exit
