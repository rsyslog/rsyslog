#!/bin/bash
# added 2018-01-22 by Rainer Gerhards; Released under ASL 2.0
. $srcdir/diag.sh init
export DO_STOP=off
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	if $msg contains "msgnum:00000000" then {
		include(text="stop" config.enabled=`echo $DO_STOP`)
	}
	action(type="omfile" template="outfmt" file="rsyslog.out.log")
}
'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh injectmsg 0 10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 9
. $srcdir/diag.sh exit
