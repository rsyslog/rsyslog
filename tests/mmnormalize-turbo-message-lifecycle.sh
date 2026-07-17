#!/bin/bash
# Verify that turbo mmnormalize CEE fields survive both MsgDup() through a
# queued ruleset and disk-queue serialization. The output files are the
# oracle: each must contain the normalized field after synchronized shutdown.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize
generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME'.spool")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="normalize")

template(name="outfmt" type="string" string="%$!host% %$!tag%\n")

ruleset(name="copy" queue.type="LinkedList") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}

ruleset(name="normalize") {
	action(type="mmnormalize" useRawMsg="on" turbo="on"
	       rule="rule=:%host:word% %tag:char-to:\\x3a%: no longer listening on %ip:ipv4%#%port:number%")
	call copy
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="outfmt"
	       queue.type="disk" queue.filename="turbo-lifecycle")
}
'
startup
tcpflood -m1 -M "\"ubuntu tag1: no longer listening on 127.168.0.1#10514\""
shutdown_when_empty
wait_shutdown
export EXPECTED='ubuntu tag1'
cmp_exact "$RSYSLOG_OUT_LOG"
cmp_exact "$RSYSLOG2_OUT_LOG"
exit_test
