#!/bin/bash
# added 2026-01-25 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list" option.jsonftree="on") {
    property(name="$!zero" outname="zero_omit" format="jsonf" dataType="number" omitIfZero="on")
    property(name="$!nonzero" outname="nonzero_omit" format="jsonf" dataType="number" omitIfZero="on")
    property(name="$!spacedzero" outname="spacedzero_omit" format="jsonf" dataType="number" omitIfZero="on")
}

set $!zero = 0;
set $!nonzero = 42;
set $!spacedzero = " 0 ";

local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
rm -f "$RSYSLOG_OUT_LOG"
startup
injectmsg
shutdown_when_empty
wait_shutdown

export EXPECTED='{"nonzero_omit":42}'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
