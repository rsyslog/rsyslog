#!/bin/bash
# added 2026-05-28 by AI agent; released under ASL 2.0
# Reproduces the list-template shape from issue #3311: an action worker renders
# several missing $! JSON fields after queueing. The oracle is exact output with
# empty field values, proving the renderer neither crashes nor invents data.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")

template(name="MTFW_CDR" type="list") {
	constant(value="{\"ReportGeneratedTime\":\"")
	property(name="$!field1")
	constant(value=" +05:30\",\"Delivered Time\":\"")
	property(name="$!field3")
	constant(value=" +05:30\",\"Record Type\":\"")
	property(name="$!field5")
	constant(value="\",\"Delivery Status\":\"")
	property(name="$!field17")
	constant(value="\",\"Failure Reason\":\"")
	property(name="$!field18")
	constant(value="\",\"Failure Reason Type\":\"")
	property(name="$!field19")
	constant(value="\",\"Service Centre\":\"")
	property(name="$!field53")
	constant(value="\",\"Vendor\":\"TELEDNA\",\"Location\":\"NOIDA\"}\n")
}

local4.* action(type="omfile"
	file="'$RSYSLOG_OUT_LOG'"
	template="MTFW_CDR"
	queue.type="LinkedList"
	queue.filename="stats_ruleset"
	queue.spoolDirectory="'${RSYSLOG_DYNNAME}'.spool"
	queue.size="100"
	queue.highWatermark="10"
	queue.lowWatermark="5"
	queue.saveOnShutdown="on"
	queue.checkpointInterval="1")
'

startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z hostname1 sender - tag [tcpflood@32473 MSGNUM="0"] data'
shutdown_when_empty
wait_shutdown

export EXPECTED='{"ReportGeneratedTime":" +05:30","Delivered Time":" +05:30","Record Type":"","Delivery Status":"","Failure Reason":"","Failure Reason Type":"","Service Centre":"","Vendor":"TELEDNA","Location":"NOIDA"}'
cmp_exact
exit_test
