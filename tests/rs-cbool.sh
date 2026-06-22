#!/bin/bash
# Verify cbool() converts common string and numeric boolean inputs into
# numeric 0/1 values that jsonf datatype="bool" renders as JSON booleans.
# The oracle is the synchronized omfile output after shutdown.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="json" type="list" option.jsonf="on") {
	property(outname="false_string" format="jsonf" name="$!false_string" datatype="bool")
	property(outname="false_upper" format="jsonf" name="$!false_upper" datatype="bool")
	property(outname="false_off" format="jsonf" name="$!false_off" datatype="bool")
	property(outname="false_no" format="jsonf" name="$!false_no" datatype="bool")
	property(outname="false_empty" format="jsonf" name="$!false_empty" datatype="bool")
	property(outname="false_number" format="jsonf" name="$!false_number" datatype="bool")
	property(outname="true_string" format="jsonf" name="$!true_string" datatype="bool")
	property(outname="true_on" format="jsonf" name="$!true_on" datatype="bool")
	property(outname="true_yes" format="jsonf" name="$!true_yes" datatype="bool")
	property(outname="true_number" format="jsonf" name="$!true_number" datatype="bool")
	property(outname="true_fallback" format="jsonf" name="$!true_fallback" datatype="bool")
	property(outname="string_false_still_true" format="jsonf" name="$!plain_false" datatype="bool")
}

set $!false_string = cbool("false");
set $!false_upper = cbool(" FALSE ");
set $!false_off = cbool("off");
set $!false_no = cbool("no");
set $!false_empty = cbool("");
set $!false_number = cbool(0);
set $!true_string = cbool("true");
set $!true_on = cbool("on");
set $!true_yes = cbool("yes");
set $!true_number = cbool(2);
set $!true_fallback = cbool("maybe");
set $!plain_false = "false";
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="json")
'
startup
shutdown_when_empty
wait_shutdown
content_check '{"false_string":false, "false_upper":false, "false_off":false, "false_no":false, "false_empty":false, "false_number":false, "true_string":true, "true_on":true, "true_yes":true, "true_number":true, "true_fallback":true, "string_false_still_true":true}'
exit_test
