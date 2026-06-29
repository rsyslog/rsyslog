#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# Written 2019-06-12 by Rainer Gerhards
# Exercise async gzip writing while repeated HUPs happen during input. The
# tcpflood pid and marker prove the background sender completed cleanly before
# shutdown and the gzip sequence oracle.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
main_queue(queue.workerThreads="10" queue.workerThreadMinimumMessages="200")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string"
	 string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
template(name="dynfile" type="string" string="'$RSYSLOG_OUT_LOG'")



:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				 zipLevel="9" ioBufferSize="16" flushOnTXEnd="on"
			         dynafile="dynfile" dynaFileCacheSize="5"
				 asyncWriting="on")

if $msg contains "msgnum" then {
	set $/file = cnum($/file) + 1;
	if $/file >= 10 then {
		set $/file = 0;
	}
}
'
startup
start_tcpflood_async TCPFLOOD_PID TCPFLOOD_MARKER -m"$NUMMESSAGES"
for i in $(seq 0 20); do
	./msleep 10
	printf '\nsending HUP %d\n' $i
	issue_HUP
done
wait_tcpflood_async "$TCPFLOOD_PID" "$TCPFLOOD_MARKER"
shutdown_when_empty
wait_shutdown
gzip_seq_check
exit_test
