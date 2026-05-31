#!/bin/bash
# added 2026-05-29 by Copilot
#
# This regression covers two runtime/msg.c behaviors:
# - nested `set $!...` updates must merge into the existing leaf object rather
#   than the root object
# - overlong JSON path components must be rejected instead of silently creating
#   truncated keys
#
# Oracle:
# - the nested node retains "keep" and gains "escape"
# - the emitted JSON tree is otherwise unchanged, so no root-level escape field
#   or truncated overlong key appears

. ${srcdir:=.}/diag.sh init
generate_conf

LONG_PATH_COMPONENT="toolong_$(printf 'x%.0s' $(seq 1 1200))"

add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%$!%\n")

local4.* {
	set $.ret = parse_json("{\"keep\":\"orig\"}", "\$.target");
	set $.ret = parse_json("{\"escape\":\"merged\"}", "\$.merge");
	set $!target!node = $.target;
	set $!target!node = $.merge;
	unset $.target;
	unset $.merge;
	set $!'"$LONG_PATH_COMPONENT"'!child = "blocked";
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

export EXPECTED='{ "target": { "node": { "keep": "orig", "escape": "merged" } } }'
cmp_exact $RSYSLOG_OUT_LOG

exit_test
