#!/bin/bash
## fromhost-port.sh
## Check that fromhost-port property records sender port
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="list") {
    property(name="$.ip_port")
    constant(value="\n")
}

set $.ip_port = $fromhost-ip & ":" & $fromhost-port;

:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m $NUMMESSAGES -w "${RSYSLOG_DYNNAME}.tcpflood-port"
shutdown_when_empty
wait_shutdown
export EXPECTED="127.0.0.1:$(cat "${RSYSLOG_DYNNAME}.tcpflood-port")"
cmp_exact
exit_test
