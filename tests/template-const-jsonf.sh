#!/bin/bash
# added 2018-02-10 by Rainer Gerhards; Released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list") {
	 constant(outname="@version" value="1" format="jsonf")
	 constant(value="\n")
}

local4.* action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
startup
. $srcdir/diag.sh injectmsg 0 1
shutdown_when_empty
wait_shutdown
. $srcdir/diag.sh content-cmp '"@version": "1"'
exit_test
