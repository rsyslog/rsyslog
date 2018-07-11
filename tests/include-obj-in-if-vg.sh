#!/bin/bash
# added 2018-01-22 by Rainer Gerhards; Released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")

if $msg contains "msgnum:" then {'
. $srcdir/diag.sh add-conf "
	include(file=\"${srcdir}/testsuites/include-std-omfile-actio*.conf\")
"
. $srcdir/diag.sh add-conf '
	continue
}
'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh injectmsg 0 10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh seq-check 0 9
. $srcdir/diag.sh exit
