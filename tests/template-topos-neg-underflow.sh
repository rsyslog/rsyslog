#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
#
# Regression test: a negative "position.to" may resolve to a position in
# front of "position.from" when the property value is short. msgGetProp()
# then computed a negative substring length, malloc(0) succeeded and the
# copy loop never terminated, which crashed rsyslog. The substring must
# simply be empty in that case.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list") {
	constant(value="short=<")
	property(name="$!short" position.from="5" position.to="-2")
	constant(value="> long=<")
	property(name="$!long" position.from="5" position.to="-2")
	constant(value=">\n")
}

set $!short = "abcde";
set $!long = "abcdefghij";

:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				 file=`echo $RSYSLOG_OUT_LOG`)
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

export EXPECTED='short=<> long=<efgh>'
cmp_exact
exit_test
