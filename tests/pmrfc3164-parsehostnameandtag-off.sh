#!/bin/bash
# Regression test for parser.parseHostnameAndTag="off".
# The global setting is parsed after the built-in RFC3164 parser is
# initialized, so the parser must read the active config at parse time. The
# oracle is the configured omfile output after shutdown: syslogtag and
# programname stay empty, and the would-be hostname/tag text remains in MSG.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(parser.parseHostnameAndTag="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="r")

template(name="outfmt" type="string"
         string="tag=[%syslogtag%] prog=[%programname%] msg=[%msg%]\n")

ruleset(name="r") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
tcpflood -m1 -M "\"<34>Oct 11 22:14:15 testhost testapp: parse-off-message\""
shutdown_when_empty
wait_shutdown

content_check "tag=[] prog=[]" "$RSYSLOG_OUT_LOG"
content_check "testhost testapp: parse-off-message" "$RSYSLOG_OUT_LOG"
exit_test
