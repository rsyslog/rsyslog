#!/bin/bash
# added 2018-01-22 by Rainer Gerhards; Released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
INCLFILE="${srcdir}/testsuites/include-std-omfile-action.conf"
export CONF_SNIPPET=`cat $INCLFILE`
printf "\nThis SNIPPET will be included via env var:\n$CONF_SNIPPET\n\nEND SNIPPET\n"
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {
	include(text=`echo $CONF_SNIPPET`)
}
'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh injectmsg 0 10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 9
. $srcdir/diag.sh exit
