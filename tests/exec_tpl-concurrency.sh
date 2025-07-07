#!/bin/bash
# Test concurrency of exec_template function with msg variables
# Added 2015-12-11 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test currently does not work on all flavors of Solaris."
export NUMMESSAGES=500000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="interim" type="string" string="%$!tree!here!nbr%")
template(name="outfmt" type="string" string="%$!interim%\n")
template(name="all-json" type="string" string="%$!%\n")

if $msg contains "msgnum:" then {
	set $!tree!here!nbr = field($msg, 58, 2);
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="all-json"
	       queue.type="linkedList")

	set $!interim = exec_template("interim");
	unset $!tree!here!nbr;
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
	       queue.type="fixedArray")
}
'
startup
tcpflood -m$NUMMESSAGES
shutdown_when_empty
wait_shutdown
seq_check
exit_test
