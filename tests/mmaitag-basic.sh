#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
## basic test for mmaitag plugin
. ${srcdir:=.}/diag.sh init
export GEMINI_MOCK_RESPONSE="NOISE,NOISE,REGULAR"

generate_conf
add_conf '
module(load="../plugins/mmaitag/.libs/mmaitag")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg% %$.aitag%\n")


if($msg contains "msgnum:00") then {
	action(type="mmaitag" provider="gemini_mock" apikey="dummy"
	       response.maxBytes="2097152"
	       request.timeoutMs="30000"
	       request.connectTimeoutMs="5000"
	       request.lowSpeedLimit="2"
	       request.lowSpeedTime="10")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
tcpflood -m 3
shutdown_when_empty
wait_shutdown
content_check "msgnum:00000000: NOISE"
content_check "msgnum:00000001: NOISE"
content_check "msgnum:00000002: REGULAR"
exit_test
