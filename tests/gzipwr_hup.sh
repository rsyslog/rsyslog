#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# Written 2019-06-12 by Rainer Gerhards
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=${NUMMESSAGES:-2000000}
export COUNT_FILE_IS_ZIPPED=yes
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export TB_TEST_TIMEOUT=180  # test is slow due to large number of messages
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string"
	 string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'")



:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				 zipLevel="9" ioBufferSize="16" flushOnTXEnd="on"
			         dynafile="dynfile")
'
startup
./tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES & # TCPFlood needs to run async!
BGPROCESS=$!
for i in $(seq 1 20); do
	printf '\nsending HUP %d\n' $i
	issue_HUP --sleep 100
done
wait $BGPROCESS
shutdown_when_empty
wait_shutdown
gzip_seq_check
exit_test
