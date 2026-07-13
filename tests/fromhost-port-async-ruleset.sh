#!/bin/bash
## fromhost-port.sh
## Check that fromhost-port property records sender port and that it
## can properly be carried over to a second asnc ruleset.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
QUEUE_TYPE=${QUEUE_TYPE:-disk}
QUEUE_EXTRA=${QUEUE_EXTRA:-}
if [ "${QUEUE_NEEDS_FILENAME:-off}" = "on" ]; then
	QUEUE_EXTRA='queue.filename="asyncq" queue.spoolDirectory="'${RSYSLOG_DYNNAME}'.spool"'
fi
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="list") {
    property(name="fromhost-port")
    constant(value="\n")
}

call async

# Note: a disk-type queue is selected to test even more rsyslog core features
ruleset(name="async" queue.type="'$QUEUE_TYPE'" '"$QUEUE_EXTRA"') {
  :msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
}
'
startup
tcpflood -m $NUMMESSAGES -w "${RSYSLOG_DYNNAME}.tcpflood-port"
shutdown_when_empty
wait_shutdown
EXPECTED="$(cat "${RSYSLOG_DYNNAME}.tcpflood-port")"
export EXPECTED
cmp_exact
exit_test
