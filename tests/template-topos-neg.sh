#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="out" type="list") {
	property(name="STRUCTURED-DATA" position.from="2" position.to="-1")
        constant(value="\n")
}

local4.debug action(type="omfile" template="out" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z hostname1 sender - tag [tcpflood@32473 MSGNUM="0"] msgnum:irrelevant'
shutdown_when_empty
wait_shutdown
export EXPECTED='tcpflood@32473 MSGNUM="0"'
cmp_exact
exit_test
