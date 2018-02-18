#!/bin/bash
# added 2018-02-10 by Rainer Gerhards; Released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="list") {
	 constant(outname="@version" value="1" format="jsonf")
	 constant(value="\n")
}

local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg 0 1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh content-cmp '"@version": "1"'
. $srcdir/diag.sh exit
