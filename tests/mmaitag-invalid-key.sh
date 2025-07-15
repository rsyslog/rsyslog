#!/bin/bash
## test gemini provider with invalid API key
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmaitag/.libs/mmaitag")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg% [%$.aitag%]\n")

if($msg contains "msgnum:00") then {
	action(type="mmaitag" provider="gemini" apikey="dummy")
}
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -m 1
shutdown_when_empty
wait_shutdown
content_check "request for a message failed" "$RSYSLOG_OUT_LOG"
exit_test
