#!/bin/bash
# This is test driver for testing asynchronous file output.
# added 2010-03-09 by Rgerhards, re-written 2019-08-15
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
# send 35555 messages, make sure file size is not a multiple of
# 4K, the buffer size!
export NUMMESSAGES=355555
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:"
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt"
		asyncWriting="on" flushOnTXEnd="off" flushInterval="2" ioBufferSize="4k")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown
seq_check
exit_test
