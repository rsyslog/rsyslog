#!/bin/bash
# released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf

socketpath=$1

if [ -z "$socketpath" ]; then
    socketpath='/var/sockets/darwin/reputation_1.sock'
fi

key=$2

if [ -z "$key" ]; then
    key='certitude'
fi

add_conf '
template(name="darwin-output" type="list") {
  property(name="$!all-json")
}

module(load="../plugins/imptcp/.libs/imptcp")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../contrib/mmdarwin/.libs/mmdarwin")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
    action(type="mmjsonparse" cookie="")
    action(type="mmdarwin" socketpath="'$socketpath'" key="'$key'" fields="!invalid" response="back" filtercode="0x72657075")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="darwin-output")
}
'

 startup
tcpflood -m 1 -M '{\"srcip\":\"2.59.151.0\"}'
shutdown_when_empty
wait_shutdown
content_check --regex '.*'$key'.*' # it should return a certitude

exit_test
