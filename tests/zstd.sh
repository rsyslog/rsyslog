#!/bin/bash
# This tests writing large data records in zst mode. We use up to 10K
# record size.
#
# added 2022-06-21 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=${NUMMESSAGES:-50000}
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
# Note: we right now use the non-compressed file as indicator for "processing complete"
#export SEQ_CHECK_FILE=$RSYSLOG_OUT_LOG.zst
add_conf '
$MaxMessageSize 10k
$MainMsgQueueTimeoutShutdown 10000

module(load="builtin:omfile" compression.driver="zstd" compression.zstd.workers="5")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n")
local0.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'.zst" template="outfmt"
		zipLevel="20" iobuffersize="64k" veryRobustZIP="off")
local0.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
# rgerhards, 2019-08-14: Note: veryRobustZip may need to be "on". Do this if the test
# still prematurely terminates. In that case it is likely that gunzip got confused
# by the missing zip close record. My initial testing shows that while gunzip emits an
# error message, everything is properly extracted. Only stressed CI runs will show how
# it works in reality.
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -m$NUMMESSAGES -r -d10000 -P129
shutdown_when_empty
wait_shutdown
seq_check 0 $((NUMMESSAGES - 1)) -E
exit_test
