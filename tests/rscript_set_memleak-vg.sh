#!/bin/bash
# A test that checks for memory leaks
# created based on real world case:
# https://github.com/rsyslog/rsyslog/issues/1376
# Copyright 2017-01-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="rcvr")

template(name="json" type="string" string="%$!%\n")
template(name="ts" type="string" string="%timestamp:::date-rfc3339%")
ruleset(name="rcvr" queue.type="LinkedList") {
	set $.index="unknown";
	set $.type="unknown";
	set $.interval=$$now & ":" & $$hour;
	set $!host_forwarded=$hostname;
	set $!host_received=$$myhostname;
	set $!time_received=$timegenerated;
	set $!@timestamp=exec_template("ts");
	action( type="omfile"
		file="rsyslog.out.log"
		template="json"
	)
}'
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh tcpflood -m5000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
# note: we check only the valgrind result, we are not really interested
# in the output data (non-standard format in any way...)
. $srcdir/diag.sh exit
