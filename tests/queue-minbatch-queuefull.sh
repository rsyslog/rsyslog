#!/bin/bash
# added 2019-01-14 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on Solaris - see https://github.com/rsyslog/rsyslog/issues/3513"
export NUMMESSAGES=10000
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				queue.type="linkedList"
				queue.size="2001"
				queue.dequeuebatchsize="2001"
				queue.mindequeuebatchsize="2001"
				queue.minDequeueBatchSize.timeout="2000"
				file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
