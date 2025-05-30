#!/bin/bash
# Added 2017-12-09 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
template(name="outfmt" type="string" string="%$!%\n")

local4.* {
	set $.ret = parse_json("{ \"c1\":\"data\" }", "\$!parsed");
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

export EXPECTED='{ "parsed": { "c1": "data" } }'
cmp_exact $RSYSLOG_OUT_LOG

exit_test
