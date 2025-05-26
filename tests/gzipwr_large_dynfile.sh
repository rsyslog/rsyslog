#!/bin/bash
# This tests writing large data records in gzip mode. We also write it to
# 5 different dynafiles, with a dynafile cache size set to 4. So this stresses
# both the input side, as well as zip writing, async writing and the dynafile
# cache logic.
#
# This test is a bit timing-dependent on the tcp reception side, so if it fails
# one may look into the timing first. The main issue is that the testbench
# currently has no good way to know if the tcp receiver is finished. This is NOT
# a problem in rsyslogd, but only of the testbench.
#
# Note that we do not yet have sufficient support for dynafiles in diag.sh,
# so we mangle some files here manually.
#
# added 2010-03-10 by Rgerhards
#
# This file is part of the rsyslog project, released  under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "FIXME: this test does not work on Solaris because of what looks like a BUG! It is just disabled here so that we can gain the benefits of a better test on other platforms. Bug on solaris must be addressed"
combine_files() {
	gunzip -c < $RSYSLOG_DYNNAME.out.0.log > $RSYSLOG_OUT_LOG
	gunzip -c < $RSYSLOG_DYNNAME.out.1.log >> $RSYSLOG_OUT_LOG
	gunzip -c < $RSYSLOG_DYNNAME.out.2.log >> $RSYSLOG_OUT_LOG
	gunzip -c < $RSYSLOG_DYNNAME.out.3.log >> $RSYSLOG_OUT_LOG
	gunzip -c < $RSYSLOG_DYNNAME.out.4.log >> $RSYSLOG_OUT_LOG
}
export NUMMESSAGES=4000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
export PRE_SEQ_CHECK_FUNC=combine_files
export SEQ_CHECK_FILE=$RSYSLOG_OUT_LOG
export SEQ_CHECK_OPTIONS=-E
generate_conf
add_conf '
global(MaxMessageSize="10k")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template outfmt,"%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n"
$template dynfile,"'$RSYSLOG_DYNNAME'.out.%msg:F,58:2%.log" # use multiple dynafiles
local0.* action(type="omfile" template="outfmt"
		zipLevel="6" ioBufferSize="256k" veryRobustZip="on"
		flushOnTXEnd="off" flushInterval="1"
		asyncWriting="on" dynaFileCacheSize="4"
		dynafile="dynfile")
'
startup
# send messages of 10.000bytes plus header max, randomized
tcpflood -m$NUMMESSAGES -r -d10000 -P129 -f5
shutdown_when_empty
wait_shutdown
seq_check
exit_test
