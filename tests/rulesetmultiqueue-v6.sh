#!/bin/bash
# Test for disk-only queue mode with v6+ config
# This tests defines three rulesets, each one with its own queue. Then, it
# sends data to them and checks the outcome. Note that we do need to
# use some custom code as the test driver framework does not (yet?)
# support multi-output-file operations.
# added 2013-11-14 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test does not work on Solaris. The overall queue
size check in imdiag requires atomics or mutexes on this platform, which we
do not use for performance reasons."
export NUMMESSAGES=60000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")

# general definition
$template outfmt,"%msg:F,58:2%\n"

# create the individual rulesets
$template dynfile1,"'$RSYSLOG_OUT_LOG'1.log" # trick to use relative path names!
ruleset(name="file1" queue.type="linkedList") {
	:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	:msg, contains, "msgnum:" ?dynfile1;outfmt
}

$template dynfile2,"'$RSYSLOG_OUT_LOG'2.log" # trick to use relative path names!
ruleset(name="file2" queue.type="linkedList") {
	:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	:msg, contains, "msgnum:" ?dynfile2;outfmt
}

$template dynfile3,"'$RSYSLOG_OUT_LOG'3.log" # trick to use relative path names!
ruleset(name="file3" queue.type="linkedList") {
	:msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	:msg, contains, "msgnum:" ?dynfile3;outfmt
}

# start listeners and bind them to rulesets
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="file1")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port2" ruleset="file2")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port3" ruleset="file3")
'
startup
assign_tcpflood_port2 "$RSYSLOG_DYNNAME.tcpflood_port2"
assign_rs_port "$RSYSLOG_DYNNAME.tcpflood_port3"
# now fill the three files (a bit sequentially, but they should
# still get their share of concurrency - to increase the chance
# we use three connections per set).
tcpflood -c3 -p$TCPFLOOD_PORT -m20000 -i0
tcpflood -c3 -p$TCPFLOOD_PORT2 -m20000 -i20000
tcpflood -c3 -p$RS_PORT -m20000 -i40000
shutdown_when_empty
wait_shutdown
# now consolidate all logs into a single one so that we can use the
seq_check # do a check on the count file - doesn't hurt as we need it anyhow
# regular check logic
cat ${RSYSLOG_OUT_LOG}1.log ${RSYSLOG_OUT_LOG}2.log ${RSYSLOG_OUT_LOG}3.log > $RSYSLOG_OUT_LOG
seq_check
exit_test
