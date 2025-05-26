#!/bin/bash
# This tests async writing functionality under a set of additional
# conditions (see config for details). This covers non-async cases as
# well. This test was created while actually hunting a bug that was
# very hard to reproduce but severe. While it had different trigger
# conditions, the set of config parameters in this test helped to find
# it quickly. As such the hope is the test will be useful in future again.
#
# NOTE WELL: The rsyslog shutdown condition is hard to get 100% right
# as due to not flushing at transaction end we cannot rely on the output
# file count as we usually do. However, we cannot avoid this as otherwise
# we loose an important trigger condition.
# added 2019-10-23 by Rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export CI_SHUTDOWN_QUEUE_EMPTY_CHECKS=30 # this test is notoriously slow...
export NUMMESSAGES=20000
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:3%,%msg:F,58:4%,%msg:F,58:5%\n")
template(name="dynfile" type="string" string="'$RSYSLOG_DYNNAME'.%msg:F,58:2%.log") # use multiple dynafiles

local0.* action(type="omfile" dynafile="dynfile" template="outfmt"
		 flushOnTXEnd="off" flushInterval="10" ioBufferSize="10k"
		 asyncWriting="on" dynaFileCacheSize="4")
'
# uncomment for debugging support:
startup
tcpflood -m$NUMMESSAGES -d1800 -P129 -f5
shutdown_when_empty
wait_shutdown
cat $RSYSLOG_DYNNAME.*.log > $RSYSLOG_OUT_LOG
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
