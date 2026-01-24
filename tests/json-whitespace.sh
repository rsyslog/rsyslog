#!/bin/bash
# added 2026-01-25 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list" option.jsonf="on") {
    property(name="$!msg" outname="msg" format="jsonf" dataType="string")
}

set $!msg = "  hello  ";

local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
rm -f "$RSYSLOG_OUT_LOG"
startup
injectmsg
shutdown_when_empty
wait_shutdown

export EXPECTED='{"msg":"  hello  "}'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
