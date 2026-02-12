#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$EscapeControlCharactersOnReceive off
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="remote"
    MultiLine="on")

template(name="outfmt" type="string" string="NEWMSG: %rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -B -I ${srcdir}/testsuites/imtcp-multiline.testdata
shutdown_when_empty
wait_shutdown
export EXPECTED='NEWMSG: <133>Msg 1
Line 2
NEWMSG: <133>Msg 2'
cmp_exact
exit_test
