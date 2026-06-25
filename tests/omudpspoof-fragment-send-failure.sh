#!/bin/bash
# Verifies that omudpspoof does not spin forever when libnet_write() fails
# while sending a fragmented message. The oracle is successful shutdown after
# injecting one oversized message while a preload shim forces every raw packet
# write to fail. Older code retried the same fragment offset indefinitely.
# This file is part of the rsyslog project, released under ASL 2.0.
echo This test must be run as root [raw socket access required]
if [ "$EUID" -ne 0 ]; then
    exit 77
fi
. ${srcdir:=.}/diag.sh init
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"

export RSYSLOG_PRELOAD=./liboverride_libnet_write_fail.so
export TCPFLOOD_EXTRA_OPTS="-b1 -W1"
export NUMMESSAGES=1
export MESSAGESIZE=65000

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omudpspoof/.libs/omudpspoof")

global(maxMessageSize="64k")
input(type="imtcp" port="'$TCPFLOOD_PORT'")

template(name="spoofaddr" type="string" string="127.0.0.1")
template(name="payload" type="string" string="%msg%\n")

if $msg contains "msgnum:" then {
	action(type="omudpspoof"
		Target="127.0.0.1"
		Port="514"
		SourceTemplate="spoofaddr"
		Template="payload"
		mtu="1500")
}
'
startup
tcpflood -m "$NUMMESSAGES" -i1 -d "$MESSAGESIZE"
shutdown_when_empty "" 10
wait_shutdown

exit_test
