#!/bin/bash
# Reproduction for parse_json() issue
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="ret: %$.ret%, parsed: %$!parsed%\n")

local4.* {
    set $.ret = parse_json("22 08 23 this is a test message", "\$!parsed");
    action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

export EXPECTED='ret: 1, parsed: '
# Since the bug is that it returns 0 and parses "22", we expect ret: 0 and parsed: 22 if it fails.
# We want it to be 1 and empty.
cmp_exact $RSYSLOG_OUT_LOG

exit_test
