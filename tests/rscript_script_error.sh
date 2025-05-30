#!/bin/bash
# Added 2017-12-09 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%$!%\n")

local4.* {
	set $!valid!serial   = parse_time("2017-10-05T01:10:11Z");
	set $!valid!error    = script_error();
	set $!invalid!serial = parse_time("not a date/time");
	set $!invalid!error  = script_error();
	set $!valid2!serial   = parse_time("2017-10-05T01:10:11Z");
	set $!valid2!error    = script_error();
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

# Our fixed and calculated expected results
export EXPECTED='{ "valid": { "serial": 1507165811, "error": 0 }, "invalid": { "serial": 0, "error": 1 }, "valid2": { "serial": 1507165811, "error": 0 } }'
cmp_exact $RSYSLOG_OUT_LOG

exit_test
