#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test currently does not work on FreeBSD"
export NUMMESSAGES=5000 #even number!
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
export SEQ_CHECK_FILE=$RSYSLOG_OUT_LOG.gz
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				 zipLevel="6" ioBufferSize="256k"
				 flushOnTXEnd="off" flushInterval="1"
				 asyncWriting="on"
			         file="'$RSYSLOG_OUT_LOG'.gz")
'
startup
tcpflood -m$((NUMMESSAGES / 2)) -P129
./msleep 5000 #wait for flush
seq_check 0 2499
tcpflood -i$((NUMMESSAGES / 2)) -m$((NUMMESSAGES / 2)) -P129
shutdown_when_empty
wait_shutdown
seq_check
exit_test
