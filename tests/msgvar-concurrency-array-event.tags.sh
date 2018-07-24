#!/bin/bash
# Test concurrency of message variables
# Added 2015-11-03 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
export TCPFLOOD_EXTRA_OPTS="-M'msg:msg: 1:2, 3:4, 5:6, 7:8 b test'"
echo ===============================================================================
echo \[msgvar-concurrency-array-event.tags.sh\]: testing concurrency of local variables
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmnormalize/.libs/mmnormalize")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%$!%\n")

#action(type="omfile" file="rsyslog2.out.log" template="outfmt" queue.type="linkedList")
action(type="mmnormalize" ruleBase="testsuites/msgvar-concurrency-array-event.tags.rulebase")
if $msg contains "msg:" then {
#	set $!tree!here!nbr = field($msg, 58, 2); # Delimiter = :
	action(type="omfile" file="rsyslog2.out.log" template="outfmt" queue.type="linkedList")
	set $!tree!here!save = $!tree!here!nbr;
	set $!tree!here!nbr = "";
	set $!tree!here!nbr = $!tree!here!save;
	action(type="omfile" file="rsyslog.out.log" template="outfmt" queue.type="linkedList")
}
'
startup
sleep 1
. $srcdir/diag.sh tcpflood -m500000
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
#seq_check 0 499999
exit_test
