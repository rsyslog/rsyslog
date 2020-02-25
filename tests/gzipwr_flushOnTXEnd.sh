#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "FreeBSD"  "This test currently does not work on FreeBSD"
export NUMMESSAGES=5000 # MUST be an even number
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" { action(type="omfile" template="outfmt"
				 zipLevel="6" ioBufferSize="256k"
				 flushOnTXEnd="on"
				 asyncWriting="on"
			         file="'$RSYSLOG_OUT_LOG'")
			    action(type="omfile" file="'$RSYSLOG_DYNNAME'.countlog")
			  }
'
startup
tcpflood -m$((NUMMESSAGES / 2)) -P129
wait_queueempty
echo test 1
wait_file_lines "$RSYSLOG_DYNNAME.countlog" $((NUMMESSAGES / 2 ))
gzip_seq_check 0 $((NUMMESSAGES / 2 - 1))
tcpflood -i$((NUMMESSAGES / 2)) -m$((NUMMESSAGES / 2)) -P129
echo test 2
wait_file_lines "$RSYSLOG_DYNNAME.countlog" $NUMMESSAGES
shutdown_when_empty
wait_shutdown
gzip_seq_check
exit_test
