#!/bin/bash
# added 2024-08-07 by ChatGPT, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# ensure recent librelp with client keepalive support
if ! pkg-config --atleast-version=1.12.0 relp 2>/dev/null ; then
    echo "SKIP: librelp without client keepalive support"
    exit 77
fi

export PORT_RCVR="$(get_free_port)"

generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp" port="'$PORT_RCVR'")

template(name="outfmt" type="list") {
        property(name="msg")
        constant(value="\n")
}
:msg, contains, "msgnum:" action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup

generate_conf 2
add_conf '
module(load="../plugins/omrelp/.libs/omrelp")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
if $msg contains "msgnum:" then action(type="omrelp" target="127.0.0.1" port="'$PORT_RCVR'" keepalive="on" keepalive.probes="5" keepalive.time="60" keepalive.interval="15")
' 2
startup 2

tcpflood -m1 -Ptcpflood_port
sleep 1

CLIENT_PID=$(cat $RSYSLOG_DYNNAME.2.pid)
if ss -tnpo | grep -E "pid=$CLIENT_PID" | grep -q keepalive; then
    :
else
    echo "keepalive not set"
    error_exit 1
fi

shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown

echo " msgnum:00000000:" | cmp - $RSYSLOG_OUT_LOG || error_exit 1

exit_test
