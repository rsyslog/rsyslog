#!/bin/bash
# Test for disk-only queue mode
# This tests defines three rulesets, each one with its own queue. Then, it
# sends data to them and checks the outcome. Note that we do need to
# use some custom code as the test driver framework does not (yet?)
# support multi-output-file operations.
# added 2009-10-30 by Rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test does not work on Solaris. The overall queue
size check in imdiag requires atomics or mutexes on this platform, which we
do not use for performance reasons."
export NUMMESSAGES=60000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp

# general definition
$template outfmt,"%msg:F,58:2%\n"

# create the individual rulesets
$ruleset file1
$RulesetCreateMainQueue on
$template dynfile1,"'$RSYSLOG_OUT_LOG'1.log" # trick to use relative path names!
:msg, contains, "msgnum:" {	?dynfile1;outfmt
				./'$RSYSLOG_OUT_LOG'
}

$ruleset file2
$RulesetCreateMainQueue on
$template dynfile2,"'$RSYSLOG_OUT_LOG'2.log" # trick to use relative path names!
:msg, contains, "msgnum:" {	?dynfile2;outfmt
				./'$RSYSLOG_OUT_LOG'
}

$ruleset file3
$RulesetCreateMainQueue on
$template dynfile3,"'$RSYSLOG_OUT_LOG'3.log" # trick to use relative path names!
:msg, contains, "msgnum:" {	?dynfile3;outfmt
				./'$RSYSLOG_OUT_LOG'
}

# start listeners and bind them to rulesets
$InputTCPServerBindRuleset file1
$InputTCPServerListenPortFile '$RSYSLOG_DYNNAME'.tcpflood_port
$InputTCPServerRun 0

$InputTCPServerBindRuleset file2
$InputTCPServerListenPortFile '$RSYSLOG_DYNNAME'.tcpflood_port2
$InputTCPServerRun 0

$InputTCPServerBindRuleset file3
$InputTCPServerListenPortFile '$RSYSLOG_DYNNAME'.tcpflood_port3
$InputTCPServerRun 0
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

# in this version of the imdiag, we do not have the capability to poll
# all queues for emptiness. So we do a sleep in the hopes that this will
# sufficiently drain the queues. This is race, but the best we currently
# can do... - rgerhards, 2009-11-05
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown
# now consolidate all logs into a single one so that we can use the
# regular check logic
cat ${RSYSLOG_OUT_LOG}1.log ${RSYSLOG_OUT_LOG}2.log ${RSYSLOG_OUT_LOG}3.log > $RSYSLOG_OUT_LOG
seq_check
exit_test
