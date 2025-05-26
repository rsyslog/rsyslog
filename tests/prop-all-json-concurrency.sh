#!/bin/bash
# Test concurrency of exec_template function with msg variables
# Added 2015-12-16 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[prop-all-json-concurrency.sh\]: testing concurrency of $!all-json variables
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="interim" type="string" string="%$!tree!here!nbr%")
template(name="outfmt" type="string" string="%$.interim%\n")
template(name="all-json" type="string" string="%$!%\n")

if $msg contains "msgnum:" then {
	set $!tree!here!nbr = field($msg, 58, 2);
	action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG` template="all-json"
	       queue.type="linkedList")

	set $.interim = $!all-json;
	unset $!tree!here!nbr;
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt"
	       queue.type="fixedArray")
}
'
startup
sleep 1
tcpflood -m500000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
seq_check 0 499999
exit_test
