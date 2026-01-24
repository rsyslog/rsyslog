#!/bin/bash
# added 2026-01-24 by Rainer Gerhards; Released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list" option.jsonf="on") {
    property(name="$!zero" outname="zero_default" format="jsonf" dataType="number")
    property(name="$!zero" outname="zero_omit" format="jsonf" dataType="number" omitIfZero="on")
    property(name="$!nonzero" outname="nonzero_omit" format="jsonf" dataType="number" omitIfZero="on")
    property(name="$!zero" outname="zero_string_omit" format="jsonf" dataType="string" omitIfZero="on")
    property(name="$!empty" outname="empty_omit" format="jsonf" dataType="number" omitIfZero="on" onEmpty="skip")
}

set $!zero = 0;
set $!nonzero = 42;
set $!empty = "";

local4.* action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown

export EXPECTED='{"zero_default":0, "nonzero_omit":42, "zero_string_omit":"0"}'
cmp_exact "$RSYSLOG_OUT_LOG"
exit_test
