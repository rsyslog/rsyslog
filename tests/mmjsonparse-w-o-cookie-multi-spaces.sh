#!/bin/bash
# add 2016-03-22 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=5000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!msgnum%\n")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

action(type="mmjsonparse" cookie="")
if $parsesuccess == "OK" then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
rm -f $RSYSLOG_OUT_LOG	# do cleanup of previous subtest
startup
tcpflood -m $NUMMESSAGES "-j \"      \""
shutdown_when_empty
wait_shutdown
seq_check
exit_test
