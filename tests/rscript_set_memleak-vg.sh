#!/bin/bash
# A test that checks for memory leaks
# created based on real world case:
# https://github.com/rsyslog/rsyslog/issues/1376
# Copyright 2017-01-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
generate_conf
add_conf '
template(name="json" type="string" string="%$!%\n")
template(name="ts" type="string" string="%timestamp:::date-rfc3339%")
ruleset(name="rcvr" queue.type="LinkedList"
	queue.timeoutShutdown="'$RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT'") {
	set $.index="unknown";
	set $.type="unknown";
	set $.interval=$$now & ":" & $$hour;
	set $!host_forwarded=$hostname;
	set $!host_received=$$myhostname;
	set $!time_received=$timegenerated;
	set $!@timestamp=exec_template("ts");
	action( type="omfile"
		file=`echo $RSYSLOG_OUT_LOG`
		template="json"
	)
}'
startup_vg
injectmsg
shutdown_when_empty
wait_shutdown_vg
check_exit_vg
# note: we check only the valgrind result, we are not really interested
# in the output data (non-standard format in any way...)
exit_test
