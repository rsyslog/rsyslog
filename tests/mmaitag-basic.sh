#!/bin/bash
## basic test for mmaitag plugin
. ${srcdir:=.}/diag.sh init
export GEMINI_MOCK_RESPONSE="NOISE,REGULAR"

generate_conf
add_conf '
module(load="../plugins/mmaitag/.libs/mmaitag")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg% %$.aitag%\n")

action(type="mmaitag" provider="gemini_mock" apikey="dummy")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
tcpflood -m 2
shutdown_when_empty
wait_shutdown
content_check "testmsg NOISE"
content_check "testmsg REGULAR"
exit_test
