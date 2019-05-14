#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# written 2019-05-10 by Rainer Gerhards
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="json" type="list" option.jsonf="on") {
	property(outname="number_0" format="jsonf" name="$!val0" datatype="number")
	property(outname="bool_0" format="jsonf" name="$!val0" datatype="bool")

	property(outname="empty" format="jsonf" name="$!empty" datatype="auto")
	property(outname="empty_skip" format="jsonf" name="$!empty" onEmpty="skip")
	property(outname="empty_null" format="jsonf" name="$!empty" onEmpty="null")
	property(outname="empty_number" format="jsonf" name="$!empty" datatype="number")

	property(outname="auto_string" format="jsonf" name="$!string" datatype="auto")

	property(outname="auto" format="jsonf" name="$!val" datatype="auto" onEmpty="null")
	property(outname="number" format="jsonf" name="$!val" datatype="number")
	property(outname="bool" format="jsonf" name="$!val" datatype="bool")
	property(outname="string" format="jsonf" name="$!val" datatype="string")
	property(outname="no_datatype" format="jsonf" name="$!val")
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
content_check '{"number_0":0, "bool_0":false, "empty":"", "empty_null":null, "empty_number":0, "auto_string":"1.2.3.4", "auto":42, "number":42, "bool":true, "string":"42", "no_datatype":"42"}'
check_not_present 'empty_skip'
exit_test
