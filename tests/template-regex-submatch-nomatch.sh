#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
#
# Regression test for
# https://github.com/rsyslog/rsyslog/issues/7399
#
# When a regex matches but the requested optional submatch did not take
# part in that match, msgGetProp() picked the configured no-match result
# and then fell through into the regular submatch extraction. All four
# regex.nomatchmode policies therefore rendered as an empty string.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list") {
	constant(value="dflt=<")
	property(name="$!value" regex.expression="([a-z]+)(-([0-9]+))?"
		 regex.type="ERE" regex.submatch="3" regex.nomatchmode="DFLT")
	constant(value="> zero=<")
	property(name="$!value" regex.expression="([a-z]+)(-([0-9]+))?"
		 regex.type="ERE" regex.submatch="3" regex.nomatchmode="ZERO")
	constant(value="> blank=<")
	property(name="$!value" regex.expression="([a-z]+)(-([0-9]+))?"
		 regex.type="ERE" regex.submatch="3" regex.nomatchmode="BLANK")
	constant(value="> field=<")
	property(name="$!value" regex.expression="([a-z]+)(-([0-9]+))?"
		 regex.type="ERE" regex.submatch="3" regex.nomatchmode="FIELD")
	constant(value="> present=<")
	property(name="$!present" regex.expression="([a-z]+)(-([0-9]+))?"
		 regex.type="ERE" regex.submatch="3" regex.nomatchmode="DFLT")
	constant(value=">\n")
}

set $!value = "abc";
set $!present = "abc-42";

:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				 file=`echo $RSYSLOG_OUT_LOG`)
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

export EXPECTED='dflt=<**NO MATCH**> zero=<0> blank=<> field=<abc> present=<42>'
cmp_exact
exit_test
