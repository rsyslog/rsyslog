#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=10
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/omsendertrack/.libs/omsendertrack")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="hostname" type="string" string="%hostname%")
action(type="omsendertrack" template="hostname")

# The following is just to find a terminating condition for message injection
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
			         file="'$RSYSLOG_OUT_LOG'")
'
startup
tcpflood -m $NUMMESSAGES
sleep 4
shutdown_when_empty
wait_shutdown
seq_check
exit_test
