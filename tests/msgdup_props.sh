#!/bin/bash
# This test checks the MsgDup() properly copies all properties.
# added 2019-06-26 by Rgerhards. Released under ASL 2.0

# create the pipe and start a background process that copies data from 
# it to the "regular" work file
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
$MainMsgQueueTimeoutShutdown 10000
template(name="all_properties" type="list") {
	property(format="jsonf" name="fromhost") constant(value="\n")
	property(format="jsonf" name="fromhost-ip") constant(value="\n")
	property(format="jsonf" name="hostname") constant(value="\n")
	property(format="jsonf" name="inputname") constant(value="\n")
	property(format="jsonf" name="msg") constant(value="\n")
	property(format="jsonf" name="msgid") constant(value="\n")
	property(format="jsonf" name="$!" outname="globalvar") constant(value="\n")
	property(format="jsonf" name="$." outname="localvar") constant(value="\n")
	property(format="jsonf" name="pri") constant(value="\n")
	property(format="jsonf" name="pri-text") constant(value="\n")
	property(format="jsonf" name="procid") constant(value="\n")
	property(format="jsonf" name="protocol-version") constant(value="\n")
	property(format="jsonf" name="rawmsg-after-pri") constant(value="\n")
	property(format="jsonf" name="rawmsg") constant(value="\n")
	property(format="jsonf" name="structured-data") constant(value="\n")
	property(format="jsonf" name="syslogtag") constant(value="\n")
	property(format="jsonf" name="timegenerated") constant(value="\n")
	property(format="jsonf" name="timegenerated" dateformat="rfc3339") constant(value="\n")
	property(format="jsonf" name="timereported") constant(value="\n")
	property(format="jsonf" name="timereported" dateformat="rfc3339") constant(value="\n")
}

ruleset(name="rs_with_queue" queue.type="LinkedList" queue.size="10000") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="all_properties")
	# works on a duplicated message thanks to the queue
}

set $!var="msg var";
set $.var="local var";
if $msg contains "msgnum:" then {
	call rs_with_queue
	action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="all_properties")
	# works on original, non-duplicated, message
}
'
startup
injectmsg 0 1 # we need only one message to check the properties
shutdown_when_empty
wait_shutdown

cmp "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG"
if [ $? -ne 0 ]; then
	printf 'ERROR: output files do not match!\n'
	printf '################# %s is:\n' "$RSYSLOG_OUT_LOG"
	cat -n "$RSYSLOG_OUT_LOG"
	printf '################# %s is:\n' "$RSYSLOG2_OUT_LOG"
	cat -n "$RSYSLOG2_OUT_LOG"
	printf '\n#################### diff is:\n'
	diff "$RSYSLOG_OUT_LOG" "$RSYSLOG2_OUT_LOG"
	error_exit  1
fi;

exit_test
