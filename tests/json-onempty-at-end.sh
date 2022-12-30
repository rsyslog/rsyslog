#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# written 2019-05-10 by Rainer Gerhards
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="json" type="list" option.jsonf="on") {
	property(outname="empty_null" format="jsonf" name="$!empty" onEmpty="null")
	property(outname="empty_skip" format="jsonf" name="$!empty" onEmpty="skip")
}

set $!val0 = 0;
set $!val = 42;
set $!empty = "";
set $!string = "1.2.3.4";
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="json")
'
startup
shutdown_when_empty
wait_shutdown
content_check '{"empty_null":null}'
check_not_present 'empty_skip'
exit_test
