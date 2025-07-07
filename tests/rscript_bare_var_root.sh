#!/bin/bash
# addd 2018-01-01 by RGerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!%\n")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="rs")

ruleset(name="rs") {
	set $!a = "TEST1";
	set $.a = "TEST-overwritten";
	set $! = $.;

	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1
shutdown_when_empty
wait_shutdown
export EXPECTED='{ "a": "TEST-overwritten" }'
echo "$EXPECTED" | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
	echo "FAIL:  $RSYSLOG_OUT_LOG content invalid:"
	cat $RSYSLOG_OUT_LOG
	echo "Expected:"
	echo "$EXPECTED"
	error_exit 1
fi;
exit_test
