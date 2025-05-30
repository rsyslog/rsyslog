#!/bin/bash
# added 2019-01-10 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
skip_platform "SunOS"  "This test currently does not work on Solaris - see https://github.com/rsyslog/rsyslog/issues/3513"
add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" action(type="omfile" template="outfmt"
				queue.type="linkedList"
				queue.mindequeuebatchsize="20"
				queue.minDequeueBatchSize.timeout="30000" # 30 sec
				file="'$RSYSLOG_OUT_LOG'")
'
#
# Note: this test is a bit tricky, as we depend on timeouts. That in turn
# calls for trouble with slow test machines. As we really want to test the
# timeout itself, we need to find a way around. What we do is use pretty
# timeout values, which should also work on slow machines. Of course, that
# also means the test is pretty slow, but that's just how it is...
# Note that we may still (hopefully very) occasional failures on some
# CI machines with very high-load.
#
startup
injectmsg 0 10
printf '%s waiting a bit to ensure batch is not yet written\n' "$(tb_timestamp)" 
sleep 5
# at this point in time, nothing must have been written and so the output
# file must not even exist.
check_file_not_exists $RSYSLOG_OUT_NAME
printf '%s %s\n' "$(tb_timestamp)" "output file does not yet exist - GOOD!"
printf '%s waiting on timeout\n' "$(tb_timestamp)" 
sleep 30
printf '%s done waiting on timeout\n' "$(tb_timestamp)" 
wait_seq_check 0 9
seq_check 0 9

printf '%s injecting new messages and waiting for shutdown\n' "$(tb_timestamp)" 
injectmsg 10 20
shutdown_when_empty
wait_shutdown
seq_check 0 29
exit_test
