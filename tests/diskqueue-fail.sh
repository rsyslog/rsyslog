#!/bin/bash
# checks that nothing bad happens if a DA (disk) queue runs out
# of configured disk space
# addd 2017-02-07 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
generate_conf
add_conf '
module(	load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="queuefail")

template(name="outfmt" type="string"
	 string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")

ruleset(
	name="queuefail"
	queue.type="Disk"
	queue.filename="fssailstocreate"
	queue.maxDiskSpace="4m"
	queue.maxfilesize="1m"
	queue.timeoutenqueue="300000"
	queue.lowwatermark="5000"
) {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
startup

tcpflood -p$TCPFLOOD_PORT -m$NUMMESSAGES

shutdown_when_empty
wait_shutdown
seq_check

exit_test
