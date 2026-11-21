#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released  under ASL 2.0
# Check that regex framing keeps the first character of each frame when the
# start regex could also match the second character (here: a leading digit).
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$EscapeControlCharactersOnReceive off
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="remote"
	framing.delimiter.regex="^[0-9]")

template(name="outfmt" type="string" string="NEWMSG: %rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -B -I ${srcdir}/testsuites/imptcp_framing_regex-first-char.testdata
shutdown_when_empty
wait_shutdown
export EXPECTED='NEWMSG: 2026-10-23 10:00:01 host app: java.lang.IllegalStateException: boom
	at com.example.Foo.bar(Foo.java:10)
	at com.example.Foo.main(Foo.java:5)
NEWMSG: 2026-10-23 10:00:02 host app: second record
NEWMSG: 2026-10-23 10:00:03 host app: third record'
cmp_exact
exit_test
