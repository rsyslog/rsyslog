#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: list-template regex.match must work when an earlier,
# skipped match ends beyond offset 32767. MsgGetProp() used to accumulate the
# per-match start offset in a 'short', which wrapped negative there, so the
# next regexec() started in front of the message buffer.
# Stimulus: one frame "<13>" + 32800 x 'a' + "1" + 16 x 'b' + "2". With
# regex "[0-9]+" and regex.match="2", match 0 is "13" (PRI), match 1 is "1"
# (ending past 32767) and match 2 is "2".
# Oracle: clean shutdown and exactly "2" in the output file.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(maxMessageSize="64k")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="rs")

template(name="outfmt" type="list") {
	property(name="rawmsg" regex.expression="[0-9]+" regex.type="ERE"
		 regex.match="2" regex.nomatchmode="DFLT")
	constant(value="\n")
}
ruleset(name="rs") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
{
	printf '<13>'
	head -c 32800 /dev/zero | tr '\0' 'a'
	printf '1bbbbbbbbbbbbbbbb2\n'
} > "$RSYSLOG_DYNNAME.input"
tcpflood -B -I "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown
export EXPECTED='2'
cmp_exact
exit_test
